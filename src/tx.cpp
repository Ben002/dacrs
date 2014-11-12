#include "serialize.h"
#include <boost/foreach.hpp>
#include "hash.h"
#include "util.h"
#include "account.h"
#include "main.h"
#include <algorithm>
#include "txdb.h"
#include "VmScript/VmScriptRun.h"
#include "core.h"
#include "miner.h"

static string txTypeArray[] = { "NULL_TXTYPE", "REG_ACCT_TX", "NORMAL_TX", "CONTRACT_TX", "FREEZE_TX",
		"REWARD_TX", "REG_SCRIPT_TX" };


bool CID::Set(const CRegID &id) {
	CDataStream ds(SER_DISK, CLIENT_VERSION);
	ds << id;
	vchData.clear();
	vchData.insert(vchData.end(), ds.begin(), ds.end());
	return true;
}
bool CID::Set(const CKeyID &id) {
	vchData.resize(20);
	memcpy(&vchData[0], &id, 20);
	return true;
}
bool CID::Set(const CPubKey &id) {
	vchData.resize(id.size());
	memcpy(&vchData[0], &id, id.size());
	return true;
}
bool CID::Set(const CUserID &userid) {
	return boost::apply_visitor(CIDVisitor(this), userid);
}

CUserID CID::GetUserId() {
	if (vchData.size() <= 10) {
		CRegID regId;
		regId.SetRegIDByCompact(vchData);
		return CUserID(regId);
	} else if (vchData.size() == 33) {
		CPubKey pubKey(vchData);
		return CUserID(pubKey);
	} else if (vchData.size() == 20) {
		uint160 data = uint160(vchData);
		CKeyID keyId(data);
		return CUserID(keyId);
	} else {
		assert(0);
	}
	return CNullID();
}

CRegID::CRegID(string strRegID) {
	nHeight = 0;
	nIndex = 0;
	vRegID.clear();
	vRegID = ::ParseHex(strRegID);
}
CRegID::CRegID(uint32_t nHeightIn, uint16_t nIndexIn) {
	nHeight = nHeightIn;
	nIndex = nIndexIn;
	vRegID.clear();
	vRegID.insert(vRegID.end(), BEGIN(nHeightIn), END(nHeightIn));
	vRegID.insert(vRegID.end(), BEGIN(nIndexIn), END(nIndexIn));
}
string CRegID::ToString() const {
	return ::HexStr(vRegID);
}
void CRegID::SetRegIDByCompact(const vector<unsigned char> &vIn) {
	CDataStream ds(vIn, SER_DISK, CLIENT_VERSION);
	ds >> *this;
}

bool CRegisterAccountTx::UpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	CAccount sourceAccount;
	CRegID accountId(nHeight, nIndex);
	CKeyID keyId = boost::get<CPubKey>(userId).GetID();
	if (!view.GetAccount(keyId, sourceAccount))
		return state.DoS(100, ERROR("UpdateAccounts() : read source addr %s account info error", accountId.ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");

	sourceAccount.publicKey = boost::get<CPubKey>(userId);
	if (llFees > 0) {
		CFund fund(llFees);
		sourceAccount.OperateAccount(MINUS_FREE, fund);
	}
	if (!view.SaveAccountInfo(accountId.GetRegID(), keyId, sourceAccount)) {
		return state.DoS(100, ERROR("UpdateAccounts() : write source addr %s account info error", accountId.ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}
	txundo.vAccountOperLog.push_back(sourceAccount.accountOperLog);
	return true;
}
bool CRegisterAccountTx::UndoUpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state,
		CTxUndo &txundo, int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	//drop account
	CRegID accountId(nHeight, nIndex);
	CAccount oldAccount;
	if (!view.GetAccount(accountId.GetRegID(), oldAccount))
		return state.DoS(100,
				ERROR("UpdateAccounts() : read secure account=%s info error", HexStr(accountId.GetRegID()).c_str()),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	CKeyID keyId;
	view.GetKeyId(accountId.GetRegID(), keyId);
	if (!oldAccount.IsEmptyValue()) {
		CPubKey empPubKey;
		oldAccount.publicKey = empPubKey;
		if (llFees > 0) {
			CAccountOperLog accountOperLog;
			if (!txundo.GetAccountOperLog(keyId, accountOperLog))
				return state.DoS(100, ERROR("UpdateAccounts() : read keyId=%s tx undo info error", keyId.GetHex()),
						UPDATE_ACCOUNT_FAIL, "bad-read-txundoinfo");
			oldAccount.UndoOperateAccount(accountOperLog);
		}
		view.SetAccount(keyId, oldAccount);
	} else {
		view.EraseAccount(keyId);
	}
	view.EraseKeyId(accountId.GetRegID());
	return true;
}
bool CRegisterAccountTx::IsValidHeight(int nCurHeight, int nTxCacheHeight) const {
	if (nValidHeight > nCurHeight + nTxCacheHeight / 2)
		return false;
	if (nValidHeight < nCurHeight - nTxCacheHeight / 2)
		return false;
	return true;
}
bool CRegisterAccountTx::GetAddress(vector<CKeyID> &vAddr, CAccountViewCache &view) {
	if (!boost::get<CPubKey>(userId).IsFullyValid()) {
		return false;
	}
	vAddr.push_back(boost::get<CPubKey>(userId).GetID());
	return true;
}
string CRegisterAccountTx::ToString(CAccountViewCache &view) const {
	string str;
	str += strprintf("txType=%s, hash=%s, ver=%d, pubkey=%s, llFees=%ld, keyid=%s, nValidHeight=%d\n",
	txTypeArray[nTxType],GetHash().ToString().c_str(), nVersion, HexStr(boost::get<CPubKey>(userId).begin(), boost::get<CPubKey>(userId).end()).c_str(), llFees, boost::get<CPubKey>(userId).GetID().GetHex(), nValidHeight);
	return str;
}
bool CRegisterAccountTx::CheckTransction(CValidationState &state, CAccountViewCache &view) {
	//check pubKey valid
	if (!boost::get<CPubKey>(userId).IsFullyValid()) {
		return state.DoS(100, ERROR("CheckTransaction() : register tx public key is invalid"), REJECT_INVALID,
				"bad-regtx-publickey");
	}

	//check signature script
	uint256 sighash = SignatureHash();
	if (!boost::get<CPubKey>(userId).Verify(sighash, signature))
		return state.DoS(100, ERROR("CheckTransaction() : register tx signature error "), REJECT_INVALID,
				"bad-regtx-signature");

	if (!MoneyRange(llFees))
		return state.DoS(100, ERROR("CheckTransaction() : register tx fee out of range"), REJECT_INVALID,
				"bad-regtx-fee-toolarge");
	return true;
}

bool CTransaction::UpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	CAccount sourceAccount;
	CAccount desAccount;
	if (!view.GetAccount(srcUserId, sourceAccount))
		return state.DoS(100,
				ERROR("UpdateAccounts() : read source addr %s account info error", HexStr(boost::get<CRegID>(srcUserId).GetRegID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");


	CID destId(desUserId);
	view.GetAccount(desUserId, desAccount);

	uint64_t minusValue = llFees + llValues;
	CFund minusFund(minusValue);
	sourceAccount.CompactAccount(nHeight - 1);
	if (!sourceAccount.OperateAccount(MINUS_FREE, minusFund))
		return state.DoS(100, ERROR("UpdateAccounts() : secure accounts insufficient funds"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	uint64_t addValue = llValues;
	CFund addFund(FREEDOM_FUND,addValue, nHeight);
	desAccount.CompactAccount(nHeight - 1);
	desAccount.OperateAccount(ADD_FREE, addFund);
	vector<CAccount> vSecureAccounts;
	vSecureAccounts.push_back(sourceAccount);
	vSecureAccounts.push_back(desAccount);
	if (!view.BatchWrite(vSecureAccounts))
		return state.DoS(100, ERROR("UpdateAccounts() : batch write secure accounts info error"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	txundo.vAccountOperLog.push_back(sourceAccount.accountOperLog);
	txundo.vAccountOperLog.push_back(desAccount.accountOperLog);
	return true;
}
bool CTransaction::UndoUpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	CAccount sourceAccount;
	CAccount desAccount;
	CID srcId(srcUserId);
	if (!view.GetAccount(srcUserId, sourceAccount))
		return state.DoS(100,
				ERROR("UpdateAccounts() : read source addr %s account info error", HexStr(srcId.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");

	CID destId(desUserId);
	if (!view.GetAccount(desUserId, desAccount)) {
		return state.DoS(100,
				ERROR("UpdateAccounts() : read destination addr %s account info error", HexStr(destId.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}

	for(auto &itemLog : txundo.vAccountOperLog){
		if(itemLog.keyID == sourceAccount.keyID) {
			sourceAccount.UndoOperateAccount(itemLog);
		}else if(itemLog.keyID == desAccount.keyID) {
			desAccount.UndoOperateAccount(itemLog);
		}
	}
	vector<CAccount> vAccounts;
	vAccounts.push_back(sourceAccount);
	vAccounts.push_back(desAccount);

	if (!view.BatchWrite(vAccounts))
		return state.DoS(100, ERROR("UpdateAccounts() : batch save accounts info error"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	return true;
}
bool CTransaction::GetAddress(vector<CKeyID> &vAddr, CAccountViewCache &view) {
	CKeyID srcKeyId;
	if (!view.GetKeyId(boost::get<CRegID>(srcUserId).GetRegID(), srcKeyId))
		return false;

	CKeyID desKeyId;
	if(desUserId.type() == typeid(CKeyID)) {
		desKeyId = boost::get<CKeyID>(desUserId);
	} else if(desUserId.type() == typeid(CRegID)){
		if (!view.GetKeyId(boost::get<CRegID>(desUserId).GetRegID(), desKeyId))
			return false;
	} else
		return false;

	vAddr.push_back(srcKeyId);
	vAddr.push_back(desKeyId);
	return true;
}
bool CTransaction::IsValidHeight(int nCurHeight, int nTxCacheHeight) const {
	if (nValidHeight > nCurHeight + nTxCacheHeight / 2)
		return false;
	if (nValidHeight < nCurHeight - nTxCacheHeight / 2)
		return false;
	return true;
}
string CTransaction::ToString(CAccountViewCache &view) const {
	string str;
	CKeyID srcKeyId, desKeyId;
	view.GetKeyId(boost::get<CRegID>(srcUserId).GetRegID(), srcKeyId);
	if (desUserId.type() == typeid(CKeyID)) {
		str += strprintf("txType=%s, hash=%s, nVersion=%d, srcAccountId=%s, llFees=%ld, llValues=%ld, desKeyId=%s, nValidHeight=%d\n",
		txTypeArray[nTxType], GetHash().ToString().c_str(), nVersion, HexStr(boost::get<CRegID>(srcUserId).GetRegID()).c_str(), llFees, llValues, boost::get<CKeyID>(desUserId).GetHex(), nValidHeight);
	} else if(desUserId.type() == typeid(CRegID)) {
		view.GetKeyId(boost::get<CRegID>(desUserId).GetRegID(), desKeyId);
		str += strprintf("txType=%s, hash=%s, nVersion=%d, srcAccountId=%s, srcKeyId=%s, llFees=%ld, llValues=%ld, desAccountId=%s, desKeyId=%s, nValidHeight=%d\n",
		txTypeArray[nTxType], GetHash().ToString().c_str(), nVersion, HexStr(boost::get<CRegID>(srcUserId).GetRegID()).c_str(), srcKeyId.GetHex(), llFees, llValues, HexStr(boost::get<CRegID>(desUserId).GetRegID()).c_str(), desKeyId.GetHex(), nValidHeight);
	}

	return str;
}
bool CTransaction::CheckTransction(CValidationState &state, CAccountViewCache &view) {
	//check source addr, destination addr
	if (srcUserId.type() != typeid(CRegID)) {
		return state.DoS(100, ERROR("CheckTransaction() : normal tx source address or des address is invalid"),
				REJECT_INVALID, "bad-normaltx-sourceaddr");
	}
	if (!MoneyRange(llFees)) {
		return state.DoS(100, ERROR("CheckTransaction() : normal tx fee out of range"), REJECT_INVALID,
				"bad-normaltx-fee-toolarge");
	}
	if (!MoneyRange(llValues)) {
		return state.DoS(100, ERROR("CheckTransaction(): normal tx value our of range"), REJECT_INVALID,
				"bad-normaltx-value-toolarge");
	}

	//check signature script
	uint256 sighash = SignatureHash();
	if (!CheckSignScript(boost::get<CRegID>(srcUserId).GetRegID(), sighash, signature, state, view)) {
		return state.DoS(100, ERROR("CheckTransaction() :CheckSignScript failed"), REJECT_INVALID,
				"bad-signscript-check");
	}

	CAccount acctDesInfo;
	if (desUserId.type() == typeid(CKeyID)) {
		if (view.GetAccount(desUserId, acctDesInfo)) {
			return state.DoS(100,
					ERROR(
							"CheckTransaction() : normal tx des account have regested, destination addr must be account id"),
					REJECT_INVALID, "bad-normal-desaddr error");
		}
	}

	return true;
}

bool CContractTransaction::UpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {

	CAccount sourceAccount;
	uint64_t minusValue = llFees;
	CFund minusFund(minusValue);
	CID id(*(vAccountRegId.rbegin()));
	if (!view.GetAccount(*(vAccountRegId.rbegin()), sourceAccount))
		return state.DoS(100,
				ERROR("UpdateAccounts() : read source addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	sourceAccount.CompactAccount(nHeight - 1);
	if (!sourceAccount.OperateAccount(MINUS_FREE, minusFund))
		return state.DoS(100, ERROR("UpdateAccounts() : secure accounts insufficient funds"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	if(!view.SetAccount(sourceAccount.keyID, sourceAccount)){
		return state.DoS(100, ERROR("UpdataAccounts() :save account%s info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-write-accountdb");

	}
	CVmScriptRun vmRun;
	std::shared_ptr<CBaseTransaction> pTx = GetNewInstance();
	uint64_t el = GetElementForBurn();
	tuple<bool, uint64_t, string> ret = vmRun.run(pTx, view, scriptCache, nHeight, el);
	if (!std::get<0>(ret))
		return state.DoS(100,
				ERROR("UpdateAccounts() : ContractTransaction UpdateAccount txhash=%s run script error:%s",
						GetHash().GetHex(), std::get<2>(ret)), UPDATE_ACCOUNT_FAIL, "run-script-error");
	vector<std::shared_ptr<CAccount> > &vAccount = vmRun.GetNewAccont();
	for (auto & itemAccount : vAccount) {
		if (!view.SetAccount(itemAccount->keyID, *itemAccount))
			return state.DoS(100,
					ERROR("UpdateAccounts() : ContractTransaction Udateaccount write secure account info error"),
					UPDATE_ACCOUNT_FAIL, "bad-write-accountdb");
		txundo.vAccountOperLog.push_back((itemAccount->accountOperLog));
	}
	txundo.vScriptOperLog = *vmRun.GetDbLog();
	return true;
}
bool CContractTransaction::UndoUpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state,
		CTxUndo &txundo, int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {

	for(auto & operacctlog : txundo.vAccountOperLog) {
		CAccount account;
		if(!view.GetAccount(operacctlog.keyID, account))  {
			return state.DoS(100,
							ERROR("UpdateAccounts() : ContractTransaction undo updateaccount read accountId= %s account info error"),
							UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
		}
		account.UndoOperateAccount(operacctlog);
		if(!view.SetAccount(operacctlog.keyID, account)) {
			return state.DoS(100,
					ERROR("UpdateAccounts() : ContractTransaction undo updateaccount write accountId= %s account info error"),
					UPDATE_ACCOUNT_FAIL, "bad-write-accountdb");
		}
	}
	for(auto &operlog : txundo.vScriptOperLog)
		if(!scriptCache.SetData(operlog.vKey, operlog.vValue))
			return state.DoS(100,
					ERROR("UpdateAccounts() : ContractTransaction undo scriptdb data error"), UPDATE_ACCOUNT_FAIL, "bad-save-scriptdb");
	return true;
}

bool CContractTransaction::GetAddress(vector<CKeyID> &vAddr, CAccountViewCache &view) {
	CKeyID keyId;
	for(auto & accountId : vAccountRegId) {
		if(!view.GetKeyId(boost::get<CRegID>(accountId).GetRegID(), keyId))
			return false;
		vAddr.push_back(keyId);
	}
	CVmScriptRun vmRun;
	std::shared_ptr<CBaseTransaction> pTx = GetNewInstance();
	uint64_t el = GetElementForBurn();
	tuple<bool, uint64_t, string> ret = vmRun.run(pTx, view, *pScriptDBTip, chainActive.Height() +1, el);
	if (!std::get<0>(ret))
		return ERROR("GetAddress()  : %s", std::get<2>(ret));

	return true;
}

bool CContractTransaction::IsValidHeight(int nCurHeight, int nTxCacheHeight) const {
	if (nValidHeight > nCurHeight + nTxCacheHeight / 2)
		return false;
	if (nValidHeight < nCurHeight - nTxCacheHeight / 2)
		return false;
	return true;
}

string CContractTransaction::ToString(CAccountViewCache &view) const {
	string str;
	string strAccountId("");
	for(auto accountId : vAccountRegId) {
		strAccountId += HexStr(boost::get<CRegID>(accountId).GetRegID());
		strAccountId += "|";
	}
	strAccountId = strAccountId.substr(0, strAccountId.length()-1);
	str += strprintf("txType=%s, hash=%s, ver=%d, vAccountRegId=%s, llFees=%ld, vContract=%s\n",
	txTypeArray[nTxType], GetHash().ToString().c_str(), nVersion, strAccountId, llFees, HexStr(vContract).c_str());
	return str;
}
bool CContractTransaction::CheckTransction(CValidationState &state, CAccountViewCache &view) {
	if (!MoneyRange(llFees)) {
		return state.DoS(100, ERROR("CheckTransaction() : appeal tx fee out of range"), REJECT_INVALID,
				"bad-appeal-fee-toolarge");
	}

	if ((vAccountRegId.size()) != (vSignature.size())) {
		return state.DoS(100, ERROR("CheckTransaction() :account size not equal to sign size"), REJECT_INVALID,
				"bad-vpre-size ");
	}

	for (int i = 0; i < vAccountRegId.size(); i++) {
		if (!CheckSignScript(boost::get<CRegID>(vAccountRegId[i]).GetRegID(), SignatureHash(), vSignature[i], state, view)) {
			return state.DoS(100, ERROR("CheckTransaction() :CheckSignScript failed"), REJECT_INVALID,
					"bad-signscript-check");
		}
	}

	//for VerifyDB checkblock return true
	if (pTxCacheTip->IsContainTx(GetHash())) {
		return true;
	}

	CVmScriptRun vmRun;
	std::shared_ptr<CBaseTransaction> pTx = GetNewInstance();

	uint64_t el = GetElementForBurn();
	tuple<bool, uint64_t, string> ret = vmRun.run(pTx, view, *pScriptDBTip, chainActive.Height() +1, el);

	if (!std::get<0>(ret))
		return state.DoS(100,
				ERROR("CheckTransaction() : ContractTransaction txhash=%s run script error,%s",
						GetHash().GetHex(), std::get<2>(ret)), UPDATE_ACCOUNT_FAIL, "run-script-error");
	return true;
}

bool CFreezeTransaction::UpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	uint64_t minusValue = llFees + llFreezeFunds;
	uint64_t freezeValue = llFreezeFunds;
	CID id(regAccountId);
	CAccount secureAccount;
	if (!view.GetAccount(regAccountId, secureAccount)) {
		return state.DoS(100, ERROR("UpdateAccounts() : read source addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}
	secureAccount.CompactAccount(nHeight - 1);
	CFund minusFund(minusValue);
	if (!secureAccount.OperateAccount(MINUS_FREE, minusFund))
		return state.DoS(100, ERROR("UpdateAccounts() : secure accounts insufficient funds"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	CFund selfFund(SELF_FREEZD_FUND,freezeValue, nUnfreezeHeight);
	if (!secureAccount.OperateAccount(ADD_SELF_FREEZD, selfFund))
		return state.DoS(100, ERROR("UpdateAccounts() : secure accounts insufficient funds"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	if (!view.SetAccount(secureAccount.keyID, secureAccount))
		return state.DoS(100, ERROR("UpdateAccounts() : batch write secure account info error"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	txundo.vAccountOperLog.push_back(secureAccount.accountOperLog);
	return true;
}
bool CFreezeTransaction::UndoUpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state,
		CTxUndo &txundo, int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	CID id(regAccountId);
	CAccount secureAccount;
	if (!view.GetAccount(regAccountId, secureAccount))
		return state.DoS(100, ERROR("UpdateAccounts() : read source addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	CAccountOperLog accountOperLog;
	if (!txundo.GetAccountOperLog(secureAccount.keyID, accountOperLog))
		return state.DoS(100, ERROR("UpdateAccounts() : read keyid=%s undo info error", secureAccount.keyID.GetHex()),
				UPDATE_ACCOUNT_FAIL, "bad-read-txundoinfo");
	secureAccount.UndoOperateAccount(accountOperLog);
	if (!view.SetAccount(secureAccount.keyID, secureAccount))
		return state.DoS(100, ERROR("UpdateAccounts() : write secure account info error"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	return true;
}
bool CFreezeTransaction::GetAddress(vector<CKeyID> &vAddr, CAccountViewCache &view) {
	CKeyID keyId;
	if (!view.GetKeyId(regAccountId, keyId))
		return false;
	vAddr.push_back(keyId);
	return true;
}
bool CFreezeTransaction::IsValidHeight(int nCurHeight, int nTxCacheHeight) const {
	if (nValidHeight > nCurHeight + nTxCacheHeight / 2)
		return false;
	if (nValidHeight < nCurHeight - nTxCacheHeight / 2)
		return false;
	return true;
}
string CFreezeTransaction::ToString(CAccountViewCache &view) const {
	string str;
	CKeyID keyId;
	view.GetKeyId(regAccountId, keyId);
	str += strprintf("txType=%s, hash=%s, ver=%d, accountId=%s, llFees=%ld, keyid=%s, llFreezeFunds=%ld, nValidHeight=%ld, nUnfreezeHeight=%d\n",
	txTypeArray[nTxType], GetHash().ToString().c_str(), nVersion, HexStr(boost::get<CRegID>(regAccountId).GetRegID()).c_str(), llFees, keyId.GetHex(), llFreezeFunds, nValidHeight, nUnfreezeHeight);
	return str;
}
bool CFreezeTransaction::CheckTransction(CValidationState &state, CAccountViewCache &view) {
	if (!MoneyRange(llFees)) {
		return state.DoS(100, ERROR("CheckTransaction() : freeze tx fee out of range"), REJECT_INVALID,
				"bad-freezetx-fee-toolarge");
	}

	if (!MoneyRange(llFreezeFunds)) {
		return state.DoS(100, ERROR("CheckTransaction(): freeze tx value our of range"), REJECT_INVALID,
				"bad-freezetx-value-toolarge");
	}

	if (!CheckSignScript(boost::get<CRegID>(regAccountId).GetRegID(), SignatureHash(), signature, state, view)) {
		return state.DoS(100, ERROR("CheckTransaction() :CheckSignScript failed"), REJECT_INVALID,
				"bad-signscript-check");
	}
	return true;
}

bool CRewardTransaction::UpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	CID id(account);
	CAccount secureAccount;
	if (!view.GetAccount(account, secureAccount)) {
		return state.DoS(100, ERROR("UpdateAccounts() : read source addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}
	LogPrint("INFO", "before rewardtx confirm account:%s\n", secureAccount.ToString());
	secureAccount.ClearAccPos(GetHash(), nHeight - 1, Params().GetIntervalPos());
	CFund fund(REWARD_FUND,rewardValue, nHeight);
	secureAccount.OperateAccount(ADD_FREE, fund);
	LogPrint("INFO", "after rewardtx confirm account:%s\n", secureAccount.ToString());
	if (!view.SetAccount(secureAccount.keyID, secureAccount))
		return state.DoS(100, ERROR("UpdateAccounts() : write secure account info error"), UPDATE_ACCOUNT_FAIL,
				"bad-save-accountdb");
	txundo.vAccountOperLog.push_back(secureAccount.accountOperLog);
	return true;
}
bool CRewardTransaction::UndoUpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state,
		CTxUndo &txundo, int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	CID id(account);
	if (account.type() != typeid(CRegID) && account.type() != typeid(CPubKey)) {
		return state.DoS(100,
				ERROR("UpdateAccounts() : account  %s error, either accountId, or pubkey",
						HexStr(id.GetID())), UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}
	CAccount secureAccount;
	if (!view.GetAccount(account, secureAccount)) {
		return state.DoS(100, ERROR("UpdateAccounts() : read source addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}
	CAccountOperLog accountOperLog;
	if (!txundo.GetAccountOperLog(secureAccount.keyID, accountOperLog))
		return state.DoS(100, ERROR("UpdateAccounts() : read keyid=%s undo info error", secureAccount.keyID.GetHex()),
				UPDATE_ACCOUNT_FAIL, "bad-read-txundoinfo");
	secureAccount.UndoOperateAccount(accountOperLog);
	if (!view.SetAccount(secureAccount.keyID, secureAccount))
		return state.DoS(100, ERROR("UpdateAccounts() : write secure account info error"), UPDATE_ACCOUNT_FAIL,
				"bad-read-accountdb");
	return true;
}
bool CRewardTransaction::GetAddress(vector<CKeyID> &vAddr, CAccountViewCache &view) {
	CKeyID keyId;
	if (account.type() == typeid(CRegID)) {
		if (!view.GetKeyId(account, keyId))
			return false;
		vAddr.push_back(keyId);
	} else if (account.type() == typeid(CPubKey)) {
		CPubKey pubKey = boost::get<CPubKey>(account);
		if (!pubKey.IsFullyValid())
			return false;
		vAddr.push_back(pubKey.GetID());
	}
	return true;
}
string CRewardTransaction::ToString(CAccountViewCache &view) const {
	string str;
	CKeyID keyId;
	view.GetKeyId(account, keyId);
	CID id(account);
	str += strprintf("txType=%s, hash=%s, ver=%d, account=%s, keyid=%s, rewardValue=%ld\n", txTypeArray[nTxType], GetHash().ToString().c_str(), nVersion, HexStr(id.GetID()).c_str(), keyId.GetHex(), rewardValue);
	return str;
}
bool CRewardTransaction::CheckTransction(CValidationState &state, CAccountViewCache &view) {
	return true;
}

bool CRegistScriptTx::UpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	LogPrint("INFO" ,"registscript UpdateAccount\n");
	CID id(regAccountId);
	CAccount secureAccount;
	if (!view.GetAccount(regAccountId, secureAccount)) {
		return state.DoS(100, ERROR("UpdateAccounts() : read regist addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}

	uint64_t minusValue = llFees;
	if (minusValue > 0) {
		CFund fund(minusValue);
		secureAccount.OperateAccount(MINUS_FREE, fund);
		txundo.vAccountOperLog.push_back(secureAccount.accountOperLog);
		if (!view.SetAccount(secureAccount.keyID, secureAccount))
			return state.DoS(100, ERROR("UpdateAccounts() : write secure account info error"), UPDATE_ACCOUNT_FAIL,
					"bad-save-accountdb");
	}
	if(script.size() == SCRIPT_ID_SIZE) {
		vector<unsigned char> vScript;
		if (!scriptCache.GetScript(script, vScript)) {
			return state.DoS(100,
					ERROR("UpdateAccounts() : Get script id=%s error", HexStr(script.begin(), script.end())),
					UPDATE_ACCOUNT_FAIL, "bad-query-scriptdb");
		}
		if(!aAuthorizate.IsNull()) {
			secureAccount.mapAuthorizate[script] = aAuthorizate;
		}
	}
	else {
		CVmScript vmScript;
		CDataStream stream(script, SER_DISK, CLIENT_VERSION);
		try {
			stream >> vmScript;
		} catch (exception& e) {
			return state.DoS(100, ERROR(("UpdateAccounts() :intial() Unserialize to vmScript error:" + string(e.what())).c_str()),
					UPDATE_ACCOUNT_FAIL, "bad-query-scriptdb");
		}
		if(!vmScript.IsValid())
			return state.DoS(100, ERROR("UpdateAccounts() : vmScript invalid"), UPDATE_ACCOUNT_FAIL, "bad-query-scriptdb");
		if (0 == nIndex)
		return true;

		CRegID scriptId(nHeight, nIndex);
		//create script account
		CKeyID keyId = Hash160(scriptId.GetRegID());
		CAccount account;
		account.keyID = keyId;
		if(!view.SaveAccountInfo(scriptId.GetRegID(), keyId, account)) {
			return state.DoS(100,
								ERROR("UpdateAccounts() : create new account script id %s script info error", HexStr(scriptId.GetRegID())),
								UPDATE_ACCOUNT_FAIL, "bad-save-scriptdb");
		}
		//save new script content
		if(!scriptCache.SetScript(scriptId.GetRegID(), script)){
			return state.DoS(100,
					ERROR("UpdateAccounts() : save script id %s script info error", HexStr(scriptId.GetRegID())),
					UPDATE_ACCOUNT_FAIL, "bad-save-scriptdb");
		}
		if(!aAuthorizate.IsNull()) {
			secureAccount.mapAuthorizate[scriptId.GetRegID()] = aAuthorizate;
		}
	}
	return true;
}
bool CRegistScriptTx::UndoUpdateAccount(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionCache &txCache, CScriptDBViewCache &scriptCache) {
	CID id(regAccountId);
	CAccount secureAccount;
	if (!view.GetAccount(regAccountId, secureAccount)) {
		return state.DoS(100, ERROR("UpdateAccounts() : read regist addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}

	if(script.size() != 6) {

		CRegID scriptId(nHeight, nIndex);
		//delete script content
		if (!scriptCache.EraseScript(scriptId.GetRegID())) {
			return state.DoS(100, ERROR("UpdateAccounts() : erase script id %s error", HexStr(scriptId.GetRegID())),
					UPDATE_ACCOUNT_FAIL, "bad-save-scriptdb");
		}
		//delete account
		if(!view.EraseKeyId(scriptId.GetRegID())){
			return state.DoS(100, ERROR("UpdateAccounts() : erase script account %s error", HexStr(scriptId.GetRegID())),
								UPDATE_ACCOUNT_FAIL, "bad-save-scriptdb");
		}
		CKeyID keyId = Hash160(scriptId.GetRegID());
		if(!view.EraseAccount(keyId)){
			return state.DoS(100, ERROR("UpdateAccounts() : erase script account %s error", HexStr(scriptId.GetRegID())),
								UPDATE_ACCOUNT_FAIL, "bad-save-scriptdb");
		}
	}
	CAccountOperLog accountOperLog;
	if (!txundo.GetAccountOperLog(secureAccount.keyID, accountOperLog))
		return state.DoS(100, ERROR("UpdateAccounts() : read keyid=%s undo info error", secureAccount.keyID.GetHex()),
				UPDATE_ACCOUNT_FAIL, "bad-read-txundoinfo");
	secureAccount.UndoOperateAccount(accountOperLog);

	if (!view.SetAccount(secureAccount.keyID, secureAccount))
		return state.DoS(100, ERROR("UpdateAccounts() : write secure account info error"), UPDATE_ACCOUNT_FAIL,
				"bad-save-accountdb");
	return true;
}
bool CRegistScriptTx::GetAddress(vector<CKeyID> &vAddr, CAccountViewCache &view) {
	CKeyID keyId;
	if (!view.GetKeyId(regAccountId, keyId))
		return false;
	vAddr.push_back(keyId);
	return true;
}
bool CRegistScriptTx::IsValidHeight(int nCurHeight, int nTxCacheHeight) const {
	if (nValidHeight > nCurHeight + nTxCacheHeight / 2)
		return false;
	if (nValidHeight < nCurHeight - nTxCacheHeight / 2)
		return false;
	return true;
}
string CRegistScriptTx::ToString(CAccountViewCache &view) const {
	string str;
	CKeyID keyId;
	view.GetKeyId(regAccountId, keyId);
	str += strprintf("txType=%s, hash=%s, ver=%d, accountId=%s, keyid=%s, llFees=%ld, nValidHeight=%d\n",
	txTypeArray[nTxType], GetHash().ToString().c_str(), nVersion, HexStr(boost::get<CRegID>(regAccountId).GetRegID()).c_str(), keyId.GetHex(), llFees, nValidHeight);
	return str;
}
bool CRegistScriptTx::CheckTransction(CValidationState &state, CAccountViewCache &view) {
	CAccount  account;
	if(!view.GetAccount(regAccountId, account)) {
		return state.DoS(100, ERROR("CheckTransaction() : register script tx get registe account info error"), REJECT_INVALID,
				"bad-read-account-info");
	}
	if(script.size() == SCRIPT_ID_SIZE) {
		vector<unsigned char> vScriptContent;
		if(!pScriptDBTip->GetScript(script, vScriptContent)) {
			return state.DoS(100,
					ERROR("CheckTransaction() : register script tx get exit script content by script reg id:%s error",
							HexStr(script.begin(), script.end())), REJECT_INVALID, "bad-read-script-info");
		}
	}

	if (!MoneyRange(llFees)) {
			return state.DoS(100, ERROR("CheckTransaction() : register script tx fee out of range"), REJECT_INVALID,
					"bad-register-script-fee-toolarge");
	}

	uint256 signhash = SignatureHash();
	if (!CheckSignScript(boost::get<CRegID>(regAccountId).GetRegID(), signhash, signature, state, view)) {
		return state.DoS(100, ERROR("CheckTransaction() :CheckSignScript failed"), REJECT_INVALID,
				"bad-signscript-check");
	}

	if (!aAuthorizate.IsValid())
		return state.DoS(100, ERROR("CheckTransaction() : Authorizate data invalid"), REJECT_INVALID, "bad-signcript-check");

	return true;
}

bool CFund::IsMergeFund(const int & nCurHeight, int &nMergeType) const {
	if (nCurHeight - nHeight > Params().GetMaxCoinDay() / Params().GetTargetSpacing()) {
		nMergeType = FREEDOM;
		return true;
	}

	switch (nFundType) {
	case REWARD_FUND:
		if (nCurHeight - nHeight > COINBASE_MATURITY) {
			nMergeType = FREEDOM_FUND;  // Merget to Freedom;
			return true;
		}
		break;
	case FREEDOM_FUND:
		return false;
	case FREEZD_FUND:
	case SELF_FREEZD_FUND:
		if (nCurHeight >= nHeight) {
			nMergeType = FREEDOM_FUND;  // Merget to Freedom;
			return true;
		}
		break;
	default:
		assert(0);
	}
	return false;
}

string CFund::ToString() const {
	string str;
	string fundTypeArray[] = { "NULL_FUNDTYPE", "FREEDOM", "REWARD_FUND", "FREEDOM_FUND", "IN_FREEZD_FUND",
			"OUT_FREEZD_FUND", "SELF_FREEZD_FUND" };
	str += strprintf("            nType=%s, uTxHash=%d, value=%ld, nHeight=%d\n",
	fundTypeArray[nFundType], HexStr(scriptID).c_str(), value, nHeight);
	//LogPrint("INFO", "%s", str.c_str());
	return str;
}

string COperFund::ToString() const {
	string str("");
	string strOperType[] = { "NULL_OPER_TYPE", "ADD_FUND", "MINUS_FUND" };
	str += strprintf("        list funds: operType=%s\n", strOperType[operType]);
	vector<CFund>::const_iterator iterFund = vFund.begin();
//	LogPrint("INFO", "        list funds: operType=%s\n", strOperType[operType]);
	for (; iterFund != vFund.end(); ++iterFund)
		str += iterFund->ToString();
	return str;
}

string CAuthorizateLog::ToString() const {
	string str("");
	str += strprintf("bvalid is %d,LastOperHeight is %d,lastCurMoney is %d,lastMaxTotalMoney is %d,scriptID is %s \n"
	, bValid,nLastOperHeight,nLastCurMaxMoneyPerDay,nLastMaxMoneyTotal,HexStr(scriptID));

	return str;
}
string CAccountOperLog::ToString() const {
	string str("");
	str += strprintf("    list oper funds: keyId=%d\n",keyID.GetHex());
	vector<COperFund>::const_iterator iterOperFund = vOperFund.begin();
//	LogPrint("INFO", "    list oper funds: keyId=%d\n", keyID.GetHex());
	for (; iterOperFund != vOperFund.end(); ++iterOperFund)
		str += iterOperFund->ToString();
	return str;
}

string CTxUndo::ToString() const {
	vector<CAccountOperLog>::const_iterator iterLog = vAccountOperLog.begin();
	string str("  list account oper Log:\n");
	for (; iterLog != vAccountOperLog.end(); ++iterLog) {
		str += iterLog->ToString();
	}
	return str;
}

bool CTxUndo::GetAccountOperLog(const CKeyID &keyId, CAccountOperLog &accountOperLog) {
	vector<CAccountOperLog>::iterator iterLog = vAccountOperLog.begin();
	for (; iterLog != vAccountOperLog.end(); ++iterLog) {
		if (iterLog->keyID == keyId) {
			accountOperLog = *iterLog;
			return true;
		}
	}
	return false;
}

void CAccount::CompactAccount(int nCurHeight) {
	MergerFund(vRewardFund, nCurHeight);
	MergerFund(vFreeze, nCurHeight);
	MergerFund(vSelfFreeze, nCurHeight);
	MergerFund(vFreedomFund, nCurHeight);
}

void CAccount::MergerFund(vector<CFund> &vFund, int nCurHeight) {
	stable_sort(vFund.begin(), vFund.end(), greater<CFund>());
	uint64_t value = 0;
	vector<CFund> vMinusFunds;
	vector<CFund> vAddFunds;
	vector<CFund>::reverse_iterator iterFund = vFund.rbegin();
	bool bMergeFund = false;
	for (; iterFund != vFund.rend();) {
		int nMergerType(0);
		if (iterFund->IsMergeFund(nCurHeight, nMergerType)) {
			bMergeFund = true;
			if (FREEDOM == nMergerType) {
				value += iterFund->value;
			} else if (FREEDOM_FUND == nMergerType) {
				CFund fund(*iterFund);
				fund.nFundType = FREEDOM_FUND;
				AddToFreedom(fund);
			} else {
				assert(0);
			}
			vMinusFunds.push_back(*iterFund);
			vFund.erase((++iterFund).base());
		}
		if (!bMergeFund) {
			break;
		} else {
			bMergeFund = false;
		}
	}

	if (value) {
		llValues += value;
		CFund addFund;
		addFund.nFundType = FREEDOM;
		addFund.value = value;
		vAddFunds.push_back(addFund);
	}
	if (!vMinusFunds.empty()) {
		COperFund log(MINUS_FUND, vMinusFunds);
		WriteOperLog(log);
	}
	if (!vAddFunds.empty()) {
		COperFund OperLog(ADD_FUND, vAddFunds);
		WriteOperLog(OperLog);
	}
}

void CAccount::WriteOperLog(const COperFund &operLog) {
//	for(auto item:operLog.vFund)
//		cout<<"bAuthorizated:"<<static_cast<int>(operLog.bAuthorizated)<<
//		"type is: "<<static_cast<int>(operLog.operType)<<"value is: "<<item.value<<endl;
	accountOperLog.InsertOperateLog(operLog);
}

void CAccount::AddToSelfFreeze(const CFund &fund, bool bWriteLog) {
	bool bMerge = false;
	for (auto& item : vSelfFreeze) {
		if (item.nHeight == fund.nHeight) {
			item.value += fund.value;
			bMerge = true;
			break;
		}
	}

	if (!bMerge)
		vSelfFreeze.push_back(fund);

	if (bWriteLog)
		WriteOperLog(ADD_FUND, fund);
}

void CAccount::AddToFreeze(const CFund &fund, bool bWriteLog) {
	bool bMerge = false;
	for (auto& item : vFreeze) {
		if (item.scriptID == fund.scriptID && item.nHeight == fund.nHeight) {
			item.value += fund.value;
			bMerge = true;
			break;
		}
	}

	if (!bMerge)
		vFreeze.push_back(fund);

	if (bWriteLog)
		WriteOperLog(ADD_FUND, fund);
}

void CAccount::AddToFreedom(const CFund &fund, bool bWriteLog) {
	int nTenDayBlocks = 10 * ((24 * 60 * 60) / Params().GetTargetSpacing());
	int nHeightPoint = fund.nHeight - fund.nHeight % nTenDayBlocks;
	vector<CFund>::iterator it = find_if(vFreedomFund.begin(), vFreedomFund.end(), [&](const CFund& fundInVector)
	{	return fundInVector.nHeight == nHeightPoint;});
	if (vFreedomFund.end() == it) {
		CFund addFund(fund);
		addFund.nHeight = nHeightPoint;
		vFreedomFund.push_back(addFund);
		if (bWriteLog)
			WriteOperLog(ADD_FUND, addFund);
		//MergerFund(vFreedomFund, nCurHeight);
	} else {
		CFund addFund(*it);
		it->value += fund.value;
		addFund.value = fund.value;
		if (bWriteLog)
			WriteOperLog(ADD_FUND, addFund);
	}
}

bool CAccount::MinusFree(const CFund &fund,bool bAuthorizated) {
	vector<CFund> vOperFund;
	uint64_t nCandidateValue = 0;
	vector<CFund>::iterator iterFound = vFreedomFund.begin();
	if (!vFreedomFund.empty()) {
		for (; iterFound != vFreedomFund.end(); ++iterFound) {
			nCandidateValue += iterFound->value;
			if (nCandidateValue >= fund.value) {
				break;
			}
		}
	}

	if (iterFound != vFreedomFund.end()) {

		uint64_t remainValue = nCandidateValue - fund.value;
		if (remainValue > 0) {
			vOperFund.insert(vOperFund.end(), vFreedomFund.begin(), iterFound);
			CFund fundMinus(*iterFound);
			fundMinus.value = iterFound->value - remainValue;
			iterFound->value = remainValue;
			vOperFund.push_back(fundMinus);
			vFreedomFund.erase(vFreedomFund.begin(), iterFound);
		}else{
			vOperFund.insert(vOperFund.end(), vFreedomFund.begin(), iterFound + 1);
			vFreedomFund.erase(vFreedomFund.begin(), iterFound + 1);
		}

		COperFund operLog(MINUS_FUND, vOperFund,bAuthorizated);
		WriteOperLog(operLog);
		return true;

	} else {
		if (llValues < fund.value - nCandidateValue)
			return false;

		CFund freedom;
		freedom.nFundType = FREEDOM;
		freedom.value = fund.value - nCandidateValue;
		llValues -= fund.value - nCandidateValue;

		vOperFund.insert(vOperFund.end(), vFreedomFund.begin(), vFreedomFund.end());
		vFreedomFund.clear();
		vOperFund.push_back(freedom);
		COperFund operLog(MINUS_FUND, vOperFund,bAuthorizated);
		WriteOperLog(operLog);

		return true;
	}

}

bool CAccount::UndoOperateAccount(const CAccountOperLog & accountOperLog) {
	bool bOverDay = false;
	if (accountOperLog.authorLog.IsLogValid())
		bOverDay = true;

	vector<COperFund>::const_reverse_iterator iterOperFundLog = accountOperLog.vOperFund.rbegin();
	for (; iterOperFundLog != accountOperLog.vOperFund.rend(); ++iterOperFundLog) {
		vector<CFund>::const_iterator iterFund = iterOperFundLog->vFund.begin();
		for (; iterFund != iterOperFundLog->vFund.end(); ++iterFund) {
			switch (iterFund->nFundType) {
			case FREEDOM:
				if (ADD_FUND == iterOperFundLog->operType) {
					assert(llValues >= iterFund->value);
					llValues -= iterFund->value;
				} else if (MINUS_FUND == iterOperFundLog->operType) {
					llValues += iterFund->value;
					if (iterOperFundLog->bAuthorizated && !bOverDay)
						UndoAuthorityOnDay(iterFund->value, accountOperLog.authorLog);
				}
				break;
			case REWARD_FUND:
				if (ADD_FUND == iterOperFundLog->operType)
					vRewardFund.erase(remove(vRewardFund.begin(), vRewardFund.end(), *iterFund), vRewardFund.end());
				else if (MINUS_FUND == iterOperFundLog->operType)
					vRewardFund.push_back(*iterFund);
				break;
			case FREEDOM_FUND:
				if (ADD_FUND == iterOperFundLog->operType) {
					auto it = find_if(vFreedomFund.begin(), vFreedomFund.end(), [&](const CFund& fundInVector) {
						if (fundInVector.nFundType== iterFund->nFundType &&
								fundInVector.nHeight == iterFund->nHeight&&
								fundInVector.value>=iterFund->value)
						return true;
					});
					assert(it != vFreedomFund.end());
					it->value -= iterFund->value;
					if (!it->value)
						vFreedomFund.erase(it);
				} else if (MINUS_FUND == iterOperFundLog->operType) {
					AddToFreedom(*iterFund, false);
					if (iterOperFundLog->bAuthorizated && !bOverDay)
						UndoAuthorityOnDay(iterFund->value, accountOperLog.authorLog);
				}

				break;
			case FREEZD_FUND:
				if (ADD_FUND == iterOperFundLog->operType) {
					auto it = find_if(vFreeze.begin(), vFreeze.end(), [&](const CFund& fundInVector) {
						if (fundInVector.nFundType== iterFund->nFundType &&
								fundInVector.nHeight == iterFund->nHeight&&
								fundInVector.scriptID == iterFund->scriptID &&
								fundInVector.value>=iterFund->value)
						return true;
					});

					assert(it != vFreeze.end());
					it->value -= iterFund->value;
					if (!it->value)
						vFreeze.erase(it);
				} else if (MINUS_FUND == iterOperFundLog->operType) {
					AddToFreeze(*iterFund, false);
				}

				break;
			case SELF_FREEZD_FUND:
				if (ADD_FUND == iterOperFundLog->operType) {
					auto it = find_if(vSelfFreeze.begin(), vSelfFreeze.end(), [&](const CFund& fundInVector) {
						if (fundInVector.nFundType== iterFund->nFundType &&
								fundInVector.nHeight == iterFund->nHeight&&
								fundInVector.value>=iterFund->value)
						return true;
					});

					assert(it != vSelfFreeze.end());
					it->value -= iterFund->value;
					if (!it->value)
						vSelfFreeze.erase(it);
				} else if (MINUS_FUND == iterOperFundLog->operType) {
					AddToSelfFreeze(*iterFund, false);
					if (iterOperFundLog->bAuthorizated && !bOverDay)
						UndoAuthorityOnDay(iterFund->value, accountOperLog.authorLog);
				}

				break;
			default:
				assert(0);
				return false;
			}
		}
	}

	if (bOverDay) {
		UndoAuthorityOverDay(accountOperLog.authorLog);
	}

	return true;
}

//caculate pos
void CAccount::ClearAccPos(uint256 hash, int prevBlockHeight, int nIntervalPos) {
	/**
	 * @todo change the  uint256 hash to uint256 &hash
	 */

	int days = 0;
	uint64_t money = 0;
	money = llValues;
	{
		COperFund acclog;
		acclog.operType = MINUS_FUND;
		{
			CFund fund(FREEDOM, 0, llValues);
			acclog.vFund.push_back(fund);
			llValues = 0;
		}
		vector<CFund>::iterator iterFund = vFreedomFund.begin();
		for (; iterFund != vFreedomFund.end();) {
			days = (prevBlockHeight - iterFund->nHeight) / nIntervalPos;
			days = min(days, 30);
			days = max(days, 0);
			if (days != 0) {
				money += iterFund->value;
				acclog.vFund.push_back(*iterFund);
				iterFund = vFreedomFund.erase(iterFund);
			} else
				++iterFund;
		}
		if (money > 0) {
			WriteOperLog(acclog);
		}
	}
	{
		if (money > 0) {
			CFund fund(FREEDOM_FUND, money, prevBlockHeight + 1);
			AddToFreedom(fund);
		}
	}
}

//caculate pos
uint64_t CAccount::GetSecureAccPos(int prevBlockHeight) const {
	uint64_t accpos = 0;
	int days = 0;

	accpos = llValues * 30;
	for (const auto &freeFund :vFreedomFund) {

		int nIntervalPos = Params().GetIntervalPos();
		assert(nIntervalPos);
		days = (prevBlockHeight - freeFund.nHeight) / nIntervalPos;
		days = min(days, 30);
		days = max(days, 0);
		if (days != 0) {
			accpos += freeFund.value * days;
		}
	}
	return accpos;
}


uint64_t CAccount::GetMatureAmount(int nCurHeight) {
	CompactAccount(nCurHeight);
	uint64_t balance = 0;

	for(auto &fund:vRewardFund) {
		balance += fund.value;
	}
	return balance;
}

uint64_t CAccount::GetForzenAmount(int nCurHeight) {
	CompactAccount(nCurHeight);
	uint64_t balance = 0;

	for (auto &fund : vFreeze) {
		balance += fund.value;
	}

	for (auto &fund : vSelfFreeze) {
		balance += fund.value;
	}
	return balance;
}

uint64_t CAccount::GetBalance(int nCurHeight) {
	CompactAccount(nCurHeight);
	uint64_t balance = llValues;

	for (auto &fund : vFreedomFund) {
		balance += fund.value;
	}
	return balance;
}

uint256 CAccount::BuildMerkleTree(int prevBlockHeight) const {
	vector<uint256> vMerkleTree;
	vMerkleTree.clear();

	for (const auto &freeFund : vFreedomFund) {
		//at least larger than 100 height
		//if (prevBlockHeight < freeFund.confirmHeight + 100) {
		vMerkleTree.push_back(uint256(freeFund.scriptID));
		//}
	}

	int j = 0;
	int nSize = vMerkleTree.size();
	for (; nSize > 1; nSize = (nSize + 1) / 2) {
		for (int i = 0; i < nSize; i += 2) {
			int i2 = min(i + 1, nSize - 1);
			vMerkleTree.push_back(
					Hash(BEGIN(vMerkleTree[j + i]), END(vMerkleTree[j + i]), BEGIN(vMerkleTree[j + i2]),
							END(vMerkleTree[j + i2])));
		}
		j += nSize;
	}
	return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
}

string CAccount::ToString() const {
	string str;
	str += strprintf("keyID=%s, publicKey=%d, values=%ld\n",
	HexStr(keyID).c_str(), HexStr(publicKey).c_str(), llValues);
	for (unsigned int i = 0; i < vRewardFund.size(); ++i) {
		str += "    " + vRewardFund[i].ToString() + "\n";
	}
	for (unsigned int i = 0; i < vFreedomFund.size(); ++i) {
		str += "    " + vFreedomFund[i].ToString() + "\n";
	}
	for (unsigned int i = 0; i < vFreeze.size(); ++i) {
		str += "    " + vFreeze[i].ToString() + "\n";
	}
	for (unsigned int i = 0; i < vSelfFreeze.size(); ++i) {
		str += "    " + vSelfFreeze[i].ToString() + "\n";
	}
	return str;
}

void CAccount::WriteOperLog(AccountOper emOperType, const CFund &fund, bool bAuthorizated) {
	vector<CFund> vFund;
	vFund.push_back(fund);
	COperFund operLog(emOperType, vFund, bAuthorizated);
	WriteOperLog(operLog);
}

bool CAccount::MinusFreezed(const CFund& fund) {
	vector<CFund>::iterator it = vFreeze.begin();
	for (; it != vFreeze.end(); it++) {
		if (it->scriptID == fund.scriptID && it->nHeight == fund.nHeight) {
			break;
		}
	}

	if (it == vFreeze.end()) {
		return false;
	}

	if (fund.value > it->value) {
		return false;
	} else {

		if (it->value > fund.value) {
			CFund logfund(*it);
			logfund.value = fund.value;
			it->value -= fund.value;
			WriteOperLog(MINUS_FUND, logfund);
		} else {
			WriteOperLog(MINUS_FUND, *it);
			vFreeze.erase(it);
		}
		return true;
	}
}

bool CAccount::MinusSelf(const CFund &fund,bool bAuthorizated) {
	vector<CFund> vOperFund;
	uint64_t nCandidateValue = 0;
	vector<CFund>::iterator iterFound = vSelfFreeze.begin();
	if (!vSelfFreeze.empty()) {
		for (; iterFound != vSelfFreeze.end(); ++iterFound) {
			nCandidateValue += iterFound->value;
			if (nCandidateValue >= fund.value) {
				break;
			}
		}
	}

	if (iterFound != vSelfFreeze.end()) {
		uint64_t remainValue = nCandidateValue - fund.value;
		if (remainValue > 0) {
			vOperFund.insert(vOperFund.end(), vSelfFreeze.begin(), iterFound);
			CFund fundMinus(*iterFound);
			fundMinus.value = iterFound->value - remainValue;
			iterFound->value = remainValue;
			vOperFund.push_back(fundMinus);
			vSelfFreeze.erase(vSelfFreeze.begin(), iterFound);
		} else {
			vOperFund.insert(vOperFund.end(), vSelfFreeze.begin(), iterFound + 1);
			vSelfFreeze.erase(vSelfFreeze.begin(), iterFound + 1);
		}

		COperFund operLog(MINUS_FUND, vOperFund,bAuthorizated);
		WriteOperLog(operLog);
		return true;
	} else {
		return false;
	}
}

bool CAccount::IsMoneyOverflow(uint64_t nAddMoney) {
	if (!MoneyRange(nAddMoney))
		return false;

	uint64_t nTotalMoney = 0;
	nTotalMoney = GetVecMoney(vFreedomFund)+GetVecMoney(vRewardFund)+GetVecMoney(vFreeze)\
			+GetVecMoney(vSelfFreeze)+llValues+nAddMoney;
	return MoneyRange(static_cast<int64_t>(nTotalMoney) );
}

uint64_t CAccount::GetVecMoney(const vector<CFund>& vFund){
	uint64_t nTotal = 0;
	for(vector<CFund>::const_iterator it = vFund.begin();it != vFund.end();it++){
		nTotal += it->value;
	}

	return nTotal;
}

CFund& CAccount::FindFund(const vector<CFund>& vFund, const vector_unsigned_char &scriptID) {
	CFund vret;
	for (vector<CFund>::const_iterator it = vFund.begin(); it != vFund.end(); it++) {
		if (it->scriptID == scriptID) {
			vret = *it;
		}
	}
	return vret;
}

bool CAccount::IsAuthorized(uint64_t nMoney, int nHeight, const vector_unsigned_char& scriptID) {
	vector<unsigned char> vscript;
	if (pScriptDBTip && !pScriptDBTip->GetScript(scriptID, vscript))
		return false;

	auto it = mapAuthorizate.find(scriptID);
	if (it == mapAuthorizate.end())
		return false;

	CAuthorizate& authorizate = it->second;
	if (authorizate.GetAuthorizeTime() < nHeight || authorizate.GetLastOperHeight() >nHeight)
		return false;

	//amount of blocks that connected into chain per day
	const uint64_t nBlocksPerDay = 24 * 60 * 60 / Params().GetTargetSpacing();
	if (authorizate.GetLastOperHeight() / nBlocksPerDay == nHeight / nBlocksPerDay) {
		if (authorizate.GetCurMaxMoneyPerDay() < nMoney)
			return false;
	} else {
		if (authorizate.GetMaxMoneyPerDay() < nMoney)
			return false;
	}

	if (authorizate.GetMaxMoneyPerTime() < nMoney || authorizate.GetMaxMoneyTotal() < nMoney)
		return false;

	return true;
}

bool CAccount::IsFundValid(OperType type, const CFund &fund, int nHeight, const vector_unsigned_char* pscriptID,
		bool bCheckAuthorized) {
	switch (type) {
	case ADD_FREE: {
		if (REWARD_FUND != fund.nFundType && FREEDOM_FUND != fund.nFundType)
			return false;
		if (!IsMoneyOverflow(fund.value))
			return false;
		break;
	}

	case ADD_SELF_FREEZD: {
		if (SELF_FREEZD_FUND != fund.nFundType)
			return false;
		if (!IsMoneyOverflow(fund.value))
			return false;
		break;
	}

	case ADD_FREEZD: {
		if (FREEZD_FUND != fund.nFundType)
			return false;
		if (!IsMoneyOverflow(fund.value))
			return false;
		break;
	}

	case MINUS_FREEZD: {
		assert(pScriptDBTip);
		vector<unsigned char> vscript;
		if (!pScriptDBTip->GetScript(fund.scriptID, vscript))
			return false;
		break;
	}

	case MINUS_FREE:
	case MINUS_SELF_FREEZD: {
		if (bCheckAuthorized && pscriptID) {
			if (!IsAuthorized(fund.value, nHeight, *pscriptID))
				return false;

			if (accountOperLog.authorLog.GetScriptID() != *pscriptID)
				accountOperLog.authorLog.SetScriptID(*pscriptID);

			if (0 == accountOperLog.authorLog.GetLastOperHeight()) {

			}
		}
		break;
	}

	default:
		assert(0);
		return false;
	}

	return true;
}

bool CAccount::OperateAccount(OperType type, const CFund &fund, int nHeight,
		const vector_unsigned_char* pscriptID,
		bool bCheckAuthorized) {
	if (keyID != accountOperLog.keyID)
		accountOperLog.keyID = keyID;

	if (!IsFundValid(type, fund, nHeight, pscriptID, bCheckAuthorized))
		return false;

	if (!fund.value)
		return true;

	bool bRet = true;
	uint64_t nOperateValue = 0;
	switch (type) {
	case ADD_FREE: {
		if (REWARD_FUND == fund.nFundType) {
			vRewardFund.push_back(fund);
			WriteOperLog(ADD_FUND, fund);
		} else
			AddToFreedom(fund);
		break;
	}

	case MINUS_FREE: {
		bRet = MinusFree(fund, bCheckAuthorized);
		break;
	}

	case ADD_SELF_FREEZD: {
		AddToSelfFreeze(fund);
		break;
	}

	case MINUS_SELF_FREEZD: {
		bRet = MinusSelf(fund, bCheckAuthorized);
		break;
	}

	case ADD_FREEZD: {
		AddToFreeze(fund);
		break;
	}

	case MINUS_FREEZD: {
		bRet = MinusFreezed(fund);
		break;
	}

	default:
		assert(0);
	}

	if ((MINUS_FREE == type || MINUS_SELF_FREEZD == type) && bCheckAuthorized && bRet)
		UpdateAuthority(nHeight, fund.value, *pscriptID);

	return bRet;
}

void CAccount::UpdateAuthority(int nHeight, uint64_t nMoney, const vector_unsigned_char& scriptID) {
	map<vector_unsigned_char, CAuthorizate>::iterator it = mapAuthorizate.find(scriptID);
	if (it == mapAuthorizate.end()) {
		assert(it != mapAuthorizate.end());
		return;
	}

	//save last operating height and scriptID
	CAuthorizate& authorizate = it->second;
	if (accountOperLog.authorLog.GetScriptID() != scriptID)
		accountOperLog.authorLog.SetScriptID(scriptID);

	if (0 == accountOperLog.authorLog.GetLastOperHeight()) {
		accountOperLog.authorLog.SetLastOperHeight(authorizate.GetLastOperHeight());
	}

	//update authority after current operate
	const uint64_t nBlocksPerDay = 24 * 60 * 60 / Params().GetTargetSpacing();
	if (authorizate.GetLastOperHeight() / nBlocksPerDay < nHeight / nBlocksPerDay) {
		CAuthorizateLog log(authorizate.GetLastOperHeight(), authorizate.GetCurMaxMoneyPerDay(),
				authorizate.GetMaxMoneyTotal(), true, scriptID);
		accountOperLog.InsertAuthorLog(log);
		authorizate.SetCurMaxMoneyPerDay(authorizate.GetMaxMoneyPerDay());
	}

	uint64_t nCurMaxMoneyPerDay = authorizate.GetCurMaxMoneyPerDay();
	uint64_t nMaxMoneyTotal = authorizate.GetMaxMoneyTotal();
	assert(nCurMaxMoneyPerDay >= nMoney && nMaxMoneyTotal >= nMoney);
	authorizate.SetCurMaxMoneyPerDay(nCurMaxMoneyPerDay - nMoney);
	authorizate.SetMaxMoneyTotal(nMaxMoneyTotal - nMoney);
	authorizate.SetLastOperHeight(static_cast<uint32_t>(nHeight));
}

void CAccount::UndoAuthorityOverDay(const CAuthorizateLog& log) {
	auto it = mapAuthorizate.find(log.GetScriptID());
	if (it == mapAuthorizate.end()) {
		assert(it != mapAuthorizate.end());
		return;
	}

	CAuthorizate& authorizate = it->second;
	authorizate.SetMaxMoneyTotal(log.GetLastMaxMoneyTotal());
	authorizate.SetCurMaxMoneyPerDay(log.GetLastCurMaxMoneyPerDay());
	authorizate.SetLastOperHeight(log.GetLastOperHeight());
}

void CAccount::UndoAuthorityOnDay(uint64_t nUndoMoney, const CAuthorizateLog& log) {
	auto it = mapAuthorizate.find(log.GetScriptID());
	if (it == mapAuthorizate.end()) {
		assert(it != mapAuthorizate.end());
		return;
	}

	CAuthorizate& authorizate = it->second;
	uint64_t nCurMaxMoneyPerDay = authorizate.GetCurMaxMoneyPerDay();
	uint64_t nNewMaxMoneyPerDay = nCurMaxMoneyPerDay + nUndoMoney;
	uint64_t nMaxMoneyTotal = authorizate.GetMaxMoneyTotal();
	uint64_t nNewMaxMoneyTotal = nMaxMoneyTotal + nUndoMoney;

	authorizate.SetCurMaxMoneyPerDay(nNewMaxMoneyPerDay);
	authorizate.SetMaxMoneyTotal(nNewMaxMoneyTotal);
	authorizate.SetLastOperHeight(accountOperLog.authorLog.GetLastOperHeight());
}

CTransactionCache::CTransactionCache(CTransactionCacheDB *pTxCacheDB) {
	base = pTxCacheDB;
}

bool CTransactionCache::IsContainBlock(const CBlock &block) {
	return (mapTxHashByBlockHash.count(block.GetHash()) > 0);
}

bool CTransactionCache::AddBlockToCache(const CBlock &block) {

	if (IsContainBlock(block)) {
		LogPrint("INFO", "the block hash:%s isn't in TxCache\n", block.GetHash().GetHex());
	} else {
		vector<uint256> vTxHash;
		vTxHash.clear();
		for (auto &ptx : block.vptx) {
			vTxHash.push_back(ptx->GetHash());
		}
		mapTxHashByBlockHash.insert(make_pair(block.GetHash(), vTxHash));
	}

	LogPrint("INFO", "mapTxHashByBlockHash size:%d\n", mapTxHashByBlockHash.size());
	for (auto &item : mapTxHashByBlockHash) {
		LogPrint("INFO", "blockhash:%s\n", item.first.GetHex());
		for (auto &txHash : item.second)
			LogPrint("INFO", "txhash:%s\n", txHash.GetHex());
	}
//	for(auto &item : mapTxHashCacheByPrev) {
//		LogPrint("INFO", "prehash:%s\n", item.first.GetHex());
//		for(auto &relayTx : item.second)
//			LogPrint("INFO", "relay tx hash:%s\n", relayTx.GetHex());
//	}
	return true;
}

bool CTransactionCache::DeleteBlockFromCache(const CBlock &block) {
	if (IsContainBlock(block)) {
		for (auto &ptx : block.vptx) {
			vector<uint256> vTxHash;
			vTxHash.clear();
			mapTxHashByBlockHash[block.GetHash()] = vTxHash;
		}
		return true;
	} else {
		LogPrint("INFO", "the block hash:%s isn't in TxCache\n", block.GetHash().GetHex());
		return false;
	}

	return true;
}

bool CTransactionCache::IsContainTx(const uint256 & txHash) {
	for (auto & item : mapTxHashByBlockHash) {
		vector<uint256>::iterator it = find(item.second.begin(), item.second.end(), txHash);
		if (it != item.second.end())
			return true;
	}
	return false;
}

const map<uint256, vector<uint256> > &CTransactionCache::GetTxHashCache(void) const {
	return mapTxHashByBlockHash;
}

bool CTransactionCache::Flush() {
	bool bRet = base->Flush(mapTxHashByBlockHash);
//	if (bRet) {
//		mapTxHashByBlockHash.clear();
//		mapTxHashCacheByPrev.clear();
//	}
	return bRet;
}

void CTransactionCache::AddTxHashCache(const uint256 & blockHash, const vector<uint256> &vTxHash) {
	mapTxHashByBlockHash[blockHash] = vTxHash;
}

bool CTransactionCache::LoadTransaction() {
	return base->LoadTransaction(mapTxHashByBlockHash);
}

void CTransactionCache::Clear() {
	mapTxHashByBlockHash.clear();
}

