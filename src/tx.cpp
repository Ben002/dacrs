#include "serialize.h"
#include <boost/foreach.hpp>
#include "hash.h"
#include "util.h"
#include "database.h"
#include "main.h"
#include <algorithm>
#include "txdb.h"
#include "vm/vmrunevn.h"
#include "core.h"
#include "miner.h"
#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"
#include "json/json_spirit_writer_template.h"
using namespace json_spirit;

list<string> listBlockAppId = boost::assign::list_of("97560-1")("96298-1")("96189-1")("95130-1")("93694-1");

static bool GetKeyId(const CAccountViewCache &view, const vector<unsigned char> &ret,
		CKeyID &cKeyId) {
	if (ret.size() == 6) {
		CRegID reg(ret);
		cKeyId = reg.getKeyID(view);
	} else if (ret.size() == 34) {
		string addr(ret.begin(), ret.end());
		cKeyId = CKeyID(addr);
	}else{
		return false;
	}
	if (cKeyId.IsEmpty())
		return false;

	return true;
}

bool CID::Set(const CRegID &id) {
	CDataStream ds(SER_DISK, g_sClientVersion);
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
bool CID::Set(const CNullID &id) {
	return true;
}
bool CID::Set(const CUserID &userid) {
	return boost::apply_visitor(CIDVisitor(this), userid);
}
CUserID CID::GetUserId() {
	if (1< vchData.size() && vchData.size() <= 10) {
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
	} else if(vchData.empty()) {
		return CNullID();
	}
	else {
		LogPrint("ERROR", "vchData:%s, len:%d\n", HexStr(vchData).c_str(), vchData.size());
		throw ios_base::failure("GetUserId error from CID");
	}
	return CNullID();
}


bool CRegID::clean()  {
	nHeight = 0 ;
	nIndex = 0 ;
	vRegID.clear();
	return true;
}
CRegID::CRegID(const vector<unsigned char>& vIn) {
	assert(vIn.size() == 6);
	vRegID = vIn;
	nHeight = 0;
	nIndex = 0;
	CDataStream ds(vIn, SER_DISK, g_sClientVersion);
	ds >> nHeight;
	ds >> nIndex;
}
bool CRegID::IsSimpleRegIdStr(const string & str)
{
	int len = str.length();
	if (len >= 3) {
		int pos = str.find('-');

		if (pos > len - 1) {
			return false;
		}
		string firtstr = str.substr(0, pos);

		if (firtstr.length() > 10 || firtstr.length() == 0) //int max is 4294967295 can not over 10
			return false;

		for (auto te : firtstr) {
			if (!isdigit(te))
				return false;
		}
		string endstr = str.substr(pos + 1);
		if (endstr.length() > 10 || endstr.length() == 0) //int max is 4294967295 can not over 10
			return false;
		for (auto te : endstr) {
			if (!isdigit(te))
				return false;
		}
		return true;
	}
	return false;
}
bool CRegID::GetKeyID(const string & str,CKeyID &keyId)
{
	CRegID te(str);
	if(te.IsEmpty())
		return false;
	keyId = te.getKeyID(*g_pAccountViewTip);
	return !keyId.IsEmpty();
}
bool CRegID::IsRegIdStr(const string & str)
 {
	if(IsSimpleRegIdStr(str)){
		return true;
	}
	else if(str.length()==12){
		return true;
	}
	return false;
}
void CRegID::SetRegID(string strRegID){
	nHeight = 0;
	nIndex = 0;
	vRegID.clear();

	if(IsSimpleRegIdStr(strRegID))
	{
		int pos = strRegID.find('-');
		nHeight = atoi(strRegID.substr(0, pos).c_str());
		nIndex = atoi(strRegID.substr(pos+1).c_str());
		vRegID.insert(vRegID.end(), BEGIN(nHeight), END(nHeight));
		vRegID.insert(vRegID.end(), BEGIN(nIndex), END(nIndex));
//		memcpy(&vRegID.at(0),&nHeight,sizeof(nHeight));
//		memcpy(&vRegID[sizeof(nHeight)],&nIndex,sizeof(nIndex));
	}
	else if(strRegID.length()==12)
	{
		vRegID = ::ParseHex(strRegID);
		memcpy(&nHeight,&vRegID[0],sizeof(nHeight));
		memcpy(&nIndex,&vRegID[sizeof(nHeight)],sizeof(nIndex));
	}
}
void CRegID::SetRegID(const vector<unsigned char>& vIn) {
	assert(vIn.size() == 6);
	vRegID = vIn;
	CDataStream ds(vIn, SER_DISK, g_sClientVersion);
	ds >> nHeight;
	ds >> nIndex;
}
CRegID::CRegID(string strRegID) {
	SetRegID(strRegID);
}
CRegID::CRegID(uint32_t nHeightIn, uint16_t nIndexIn) {
	nHeight = nHeightIn;
	nIndex = nIndexIn;
	vRegID.clear();
	vRegID.insert(vRegID.end(), BEGIN(nHeightIn), END(nHeightIn));
	vRegID.insert(vRegID.end(), BEGIN(nIndexIn), END(nIndexIn));
}
string CRegID::ToString() const {
//	if(!IsEmpty())
//	return ::HexStr(vRegID);
	if(!IsEmpty())
	  return  strprintf("%d-%d",nHeight,nIndex);
	return string(" ");
}
CKeyID CRegID::getKeyID(const CAccountViewCache &view)const
{
	CKeyID ret;
	CAccountViewCache(view).GetKeyId(*this,ret);
	return ret;
}
void CRegID::SetRegIDByCompact(const vector<unsigned char> &vIn) {
	if(vIn.size()>0)
	{
		CDataStream ds(vIn, SER_DISK, g_sClientVersion);
		ds >> *this;
	}
	else
	{
		clean();
	}
}


bool CBaseTransaction::IsValidHeight(int nCurHeight, int nTxCacheHeight) const
{
	if(REWARD_TX == m_chTxType)
		return true;
	if (m_nValidHeight > nCurHeight + nTxCacheHeight / 2)
			return false;
	if (m_nValidHeight < nCurHeight - nTxCacheHeight / 2)
			return false;
	return true;
}
bool CBaseTransaction::UndoExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB) {
	vector<CAccountLog>::reverse_iterator rIterAccountLog = txundo.vAccountLog.rbegin();
	for (; rIterAccountLog != txundo.vAccountLog.rend(); ++rIterAccountLog) {
		CAccount account;
		CUserID userId = rIterAccountLog->keyID;
		if (!view.GetAccount(userId, account)) {
			return state.DoS(100, ERRORMSG("UndoExecuteTx() : CBaseTransaction UndoExecuteTx, undo ExecuteTx read accountId= %s account info error"),
					UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
		}
		if (!account.UndoOperateAccount(*rIterAccountLog)) {
			return state.DoS(100, ERRORMSG("UndoExecuteTx() : CBaseTransaction UndoExecuteTx, undo UndoOperateAccount failed"), UPDATE_ACCOUNT_FAIL,
					"undo-operate-account-failed");
		}
		if (EM_COMMON_TX == m_chTxType
				&& (account.IsEmptyValue()
						&& (!account.PublicKey.IsFullyValid() || account.PublicKey.GetKeyID() != account.keyID))) {
			view.EraseAccount(userId);
		} else {
			if (!view.SetAccount(userId, account)) {
				return state.DoS(100,
						ERRORMSG("UndoExecuteTx() : CBaseTransaction UndoExecuteTx, undo ExecuteTx write accountId= %s account info error"),
						UPDATE_ACCOUNT_FAIL, "bad-write-accountdb");
			}
		}
	}
	vector<CScriptDBOperLog>::reverse_iterator rIterScriptDBLog = txundo.vScriptOperLog.rbegin();
	for (; rIterScriptDBLog != txundo.vScriptOperLog.rend(); ++rIterScriptDBLog) {
		if (!scriptDB.UndoScriptData(rIterScriptDBLog->vKey, rIterScriptDBLog->vValue))
			return state.DoS(100, ERRORMSG("UndoExecuteTx() : CBaseTransaction UndoExecuteTx, undo scriptdb data error"), UPDATE_ACCOUNT_FAIL,
					"bad-save-scriptdb");
	}
	if(EM_CONTRACT_TX == m_chTxType) {
		if (!scriptDB.EraseTxRelAccout(GetHash()))
			return state.DoS(100, ERRORMSG("UndoExecuteTx() : CBaseTransaction UndoExecuteTx, erase tx rel account error"), UPDATE_ACCOUNT_FAIL,
							"bad-save-scriptdb");
	}
	return true;
}
uint64_t CBaseTransaction::GetFuel(int nfuelRate) {
	uint64_t llFuel = ceil(nRunStep/100.0f) * nfuelRate;
	if(REG_APP_TX == m_chTxType) {
		if (g_cChainActive.Tip()->m_nHeight > nRegAppFuel2FeeForkHeight) {
			llFuel = 0;
		} else {
			if (llFuel < 1 * COIN) {
				llFuel = 1 * COIN;
			}
		}
	}
	return llFuel;
}

string CBaseTransaction::txTypeArray[6] = { "NULL_TXTYPE", "REWARD_TX", "REG_ACCT_TX", "EM_COMMON_TX", "EM_CONTRACT_TX", "REG_APP_TX"};

int CBaseTransaction::GetFuelRate(CScriptDBViewCache &scriptDB) {
	if(0 == nFuelRate) {
		ST_DiskTxPos postx;
		if (scriptDB.ReadTxIndex(GetHash(), postx)) {
			CAutoFile file(OpenBlockFile(postx, true), SER_DISK, g_sClientVersion);
			CBlockHeader header;
			try {
				file >> header;
			} catch (std::exception &e) {
				return ERRORMSG("%s : Deserialize or I/O error - %s", __func__, e.what());
			}
			nFuelRate = header.GetFuelRate();
		}
		else {
			nFuelRate = GetElementForBurn(g_cChainActive.Tip());
		}
	}
	return nFuelRate;
}

bool CRegisterAccountTx::ExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB) {
	CAccount account;
	CRegID regId(nHeight, nIndex);
	CKeyID keyId = boost::get<CPubKey>(m_cUserId).GetKeyID();
	if (!view.GetAccount(m_cUserId, account))
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAccountTx ExecuteTx, read source keyId %s account info error", keyId.ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	CAccountLog acctLog(account);
	if(account.PublicKey.IsFullyValid() && account.PublicKey.GetKeyID() == keyId) {
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAccountTx ExecuteTx, read source keyId %s duplicate register", keyId.ToString()),
					UPDATE_ACCOUNT_FAIL, "duplicate-register-account");
	}
	account.PublicKey = boost::get<CPubKey>(m_cUserId);
	if (m_llFees > 0) {
		if(!account.OperateAccount(MINUS_FREE, m_llFees, nHeight))
			return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAccountTx ExecuteTx, not sufficient funds in account, keyid=%s", keyId.ToString()),
					UPDATE_ACCOUNT_FAIL, "not-sufficiect-funds");
	}

	account.m_cRegID = regId;
	if (typeid(CPubKey) == m_cMinerId.type()) {
		account.MinerPKey = boost::get<CPubKey>(m_cMinerId);
		if (account.MinerPKey.IsValid() && !account.MinerPKey.IsFullyValid()) {
			return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAccountTx ExecuteTx, MinerPKey:%s Is Invalid", account.MinerPKey.ToString()),
					UPDATE_ACCOUNT_FAIL, "MinerPKey Is Invalid");
		}
	}

	if (!view.SaveAccountInfo(regId, keyId, account)) {
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAccountTx ExecuteTx, write source addr %s account info error", regId.ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}
	txundo.vAccountLog.push_back(acctLog);
	txundo.txHash = GetHash();
	if(SysCfg().GetAddressToTxFlag()) {
		CScriptDBOperLog operAddressToTxLog;
		CKeyID sendKeyId;
		if(!view.GetKeyId(m_cUserId, sendKeyId)) {
			return ERRORMSG("ExecuteTx() : CRegisterAccountTx ExecuteTx, get keyid by m_cUserId error!");
		}
		if(!scriptDB.SetTxHashByAddress(sendKeyId, nHeight, nIndex+1, txundo.txHash.GetHex(), operAddressToTxLog))
			return false;
		txundo.vScriptOperLog.push_back(operAddressToTxLog);
	}
	return true;
}
bool CRegisterAccountTx::UndoExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state,
		CTxUndo &txundo, int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB) {
	//drop account
	CRegID accountId(nHeight, nIndex);
	CAccount oldAccount;
	if (!view.GetAccount(accountId, oldAccount))
		return state.DoS(100,
				ERRORMSG("ExecuteTx() : CRegisterAccountTx UndoExecuteTx, read secure account=%s info error", accountId.ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	CKeyID keyId;
	view.GetKeyId(accountId, keyId);

	if (m_llFees > 0) {
		CAccountLog accountLog;
		if (!txundo.GetAccountOperLog(keyId, accountLog))
			return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAccountTx UndoExecuteTx, read keyId=%s tx undo info error", keyId.GetHex()),
					UPDATE_ACCOUNT_FAIL, "bad-read-txundoinfo");
		oldAccount.UndoOperateAccount(accountLog);
	}

	if (!oldAccount.IsEmptyValue()) {
		CPubKey empPubKey;
		oldAccount.PublicKey = empPubKey;
		oldAccount.MinerPKey = empPubKey;
		CUserID userId(keyId);
		view.SetAccount(userId, oldAccount);
	} else {
		view.EraseAccount(m_cUserId);
	}
	view.EraseId(accountId);
	return true;
}
bool CRegisterAccountTx::GetAddress(set<CKeyID> &vAddr, CAccountViewCache &view, CScriptDBViewCache &scriptDB) {
	if (!boost::get<CPubKey>(m_cUserId).IsFullyValid()) {
		return false;
	}
	vAddr.insert(boost::get<CPubKey>(m_cUserId).GetKeyID());
	return true;
}
string CRegisterAccountTx::ToString(CAccountViewCache &view) const {
	string str;
	str += strprintf("txType=%s, hash=%s, ver=%d, pubkey=%s, m_llFees=%ld, keyid=%s, m_nValidHeight=%d\n",
	txTypeArray[m_chTxType],GetHash().ToString().c_str(), nVersion, boost::get<CPubKey>(m_cUserId).ToString(), m_llFees, boost::get<CPubKey>(m_cUserId).GetKeyID().ToAddress(), m_nValidHeight);
	return str;
}
Object CRegisterAccountTx::ToJSON(const CAccountViewCache &AccountView) const{
	Object result;

	result.push_back(Pair("hash", GetHash().GetHex()));
	result.push_back(Pair("txtype", txTypeArray[m_chTxType]));
	result.push_back(Pair("ver", nVersion));
	result.push_back(Pair("addr", boost::get<CPubKey>(m_cUserId).GetKeyID().ToAddress()));
	CID id(m_cUserId);
	CID minerIdTemp(m_cMinerId);
	result.push_back(Pair("pubkey", HexStr(id.GetID())));
	result.push_back(Pair("miner_pubkey", HexStr(minerIdTemp.GetID())));
	result.push_back(Pair("fees", m_llFees));
	result.push_back(Pair("height", m_nValidHeight));
   return result;
}
bool CRegisterAccountTx::CheckTransction(CValidationState &state, CAccountViewCache &view, CScriptDBViewCache &scriptDB) {

	if (m_cUserId.type() != typeid(CPubKey)) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAppTx m_cUserId must be CPubKey"), REJECT_INVALID,
				"userid-type-error");
	}

	if ((m_cMinerId.type() != typeid(CPubKey)) && (m_cMinerId.type() != typeid(CNullID))) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAppTx m_cMinerId must be CPubKey or CNullID"), REJECT_INVALID,
				"minerid-type-error");
	}

	//check pubKey valid
	if (!boost::get<CPubKey>(m_cUserId).IsFullyValid()) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAccountTx CheckTransction, register tx public key is invalid"), REJECT_INVALID,
				"bad-regtx-publickey");
	}

	//check m_vchSignature script
	uint256 sighash = SignatureHash();
	if(!CheckSignScript(sighash, m_vchSignature, boost::get<CPubKey>(m_cUserId))) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAccountTx CheckTransction, register tx m_vchSignature error "), REJECT_INVALID,
				"bad-regtx-m_vchSignature");
	}

	if (!MoneyRange(m_llFees))
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAccountTx CheckTransction, register tx fee out of range"), REJECT_INVALID,
				"bad-regtx-fee-toolarge");
	return true;
}

uint256 CRegisterAccountTx::GetHash() const {
	if (nTxVersion2 == nVersion) {
		return SignatureHash();
	}
	return std::move(SerializeHash(*this));
}
uint256 CRegisterAccountTx::SignatureHash() const {
	CHashWriter ss(SER_GETHASH, 0);
	CID userPubkey(m_cUserId);
	CID minerPubkey(m_cMinerId);
	if (nTxVersion2 == nVersion) {
		ss << VARINT(nVersion) << m_chTxType << VARINT(m_nValidHeight) << userPubkey << minerPubkey << VARINT(m_llFees);
	} else {
		ss << VARINT(nVersion) << m_chTxType << userPubkey << minerPubkey << VARINT(m_llFees) << VARINT(m_nValidHeight);
	}
	return ss.GetHash();
}

bool CTransaction::ExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB) {
	CAccount srcAcct;
	CAccount desAcct;
	CAccountLog desAcctLog;
	uint64_t minusValue = m_ullFees+m_ullValues;
	if (!view.GetAccount(m_cSrcRegId, srcAcct))
		return state.DoS(100,
				ERRORMSG("ExecuteTx() : CTransaction ExecuteTx, read source addr %s account info error", boost::get<CRegID>(m_cSrcRegId).ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	CAccountLog srcAcctLog(srcAcct);
	if (!srcAcct.OperateAccount(MINUS_FREE, minusValue, nHeight))
		return state.DoS(100, ERRORMSG("ExecuteTx() : CTransaction ExecuteTx, accounts insufficient funds"), UPDATE_ACCOUNT_FAIL,
				"operate-minus-account-failed");
	CUserID userId = srcAcct.keyID;
	if(!view.SetAccount(userId, srcAcct)){
		return state.DoS(100, ERRORMSG("UpdataAccounts() :CTransaction ExecuteTx, save account%s info error",  boost::get<CRegID>(m_cSrcRegId).ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-write-accountdb");
	}

	uint64_t addValue = m_ullValues;
	if(!view.GetAccount(m_cDesUserId, desAcct)) {
		if((EM_COMMON_TX == m_chTxType) && (m_cDesUserId.type() == typeid(CKeyID))) {  //Ŀ�ĵ�ַ�˻�������
			desAcct.keyID = boost::get<CKeyID>(m_cDesUserId);
			desAcctLog.keyID = desAcct.keyID;
		}
		else {
			return state.DoS(100, ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, get account info failed by regid:%s", boost::get<CRegID>(m_cDesUserId).ToString()), UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
		}
	}
	else{
		desAcctLog.SetValue(desAcct);
	}
	if (!desAcct.OperateAccount(ADD_FREE, addValue, nHeight)) {
		return state.DoS(100, ERRORMSG("ExecuteTx() : CTransaction ExecuteTx, operate accounts error"), UPDATE_ACCOUNT_FAIL,
				"operate-add-account-failed");
	}
	if (!view.SetAccount(m_cDesUserId, desAcct)) {
		return state.DoS(100, ERRORMSG("ExecuteTx() : CTransaction ExecuteTx, save account error, kyeId=%s", desAcct.keyID.ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-save-account");
	}
	txundo.vAccountLog.push_back(srcAcctLog);
	txundo.vAccountLog.push_back(desAcctLog);

	if (EM_CONTRACT_TX == m_chTxType) {

		if(nHeight>nLimiteAppHeight &&  std::find(listBlockAppId.begin(), listBlockAppId.end(), boost::get<CRegID>(m_cDesUserId).ToString())!=listBlockAppId.end()) {
			return state.DoS(100, ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, destination app id error, RegId=%s", boost::get<CRegID>(m_cDesUserId).ToString()),
									UPDATE_ACCOUNT_FAIL, "bad-read-account");
		}
		vector<unsigned char> vScript;
		if(!scriptDB.GetScript(boost::get<CRegID>(m_cDesUserId), vScript)) {
			return state.DoS(100, ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, read account faild, RegId=%s", boost::get<CRegID>(m_cDesUserId).ToString()),
						UPDATE_ACCOUNT_FAIL, "bad-read-account");
		}
		CVmRunEvn vmRunEvn;
		std::shared_ptr<CBaseTransaction> pTx = GetNewInstance();
		uint64_t el = GetFuelRate(scriptDB);
		int64_t llTime = GetTimeMillis();
		tuple<bool, uint64_t, string> ret = vmRunEvn.run(pTx, view, scriptDB, nHeight, el, nRunStep);
		if (!std::get<0>(ret))
			return state.DoS(100,
					ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, txhash=%s run script error:%s",
							GetHash().GetHex(), std::get<2>(ret)), UPDATE_ACCOUNT_FAIL, "run-script-error");
		LogPrint("EM_CONTRACT_TX", "execute contract elapse:%lld, txhash=%s\n", GetTimeMillis() - llTime,
				GetHash().GetHex());
		set<CKeyID> vAddress;
		vector<std::shared_ptr<CAccount> > &vAccount = vmRunEvn.GetNewAccont();
		for (auto & itemAccount : vAccount) {  //���¶�Ӧ�ĺ�Լ���׵��˻���Ϣ
			vAddress.insert(itemAccount->keyID);
			userId = itemAccount->keyID;
			CAccount oldAcct;
			if(!view.GetAccount(userId, oldAcct)) {
				if(!itemAccount->keyID.IsNull()) {  //��Լ��δ������ת�˼�¼��ַת��
					oldAcct.keyID = itemAccount->keyID;
				}else {
					return state.DoS(100,
							ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, read account info error"),
							UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
				}
			}
			CAccountLog oldAcctLog(oldAcct);
			if (!view.SetAccount(userId, *itemAccount))
				return state.DoS(100,
						ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, write account info error"),
						UPDATE_ACCOUNT_FAIL, "bad-write-accountdb");
			txundo.vAccountLog.push_back(oldAcctLog);
		}
		txundo.vScriptOperLog.insert(txundo.vScriptOperLog.end(), vmRunEvn.GetDbLog()->begin(), vmRunEvn.GetDbLog()->end());
		vector<std::shared_ptr<CAppUserAccout> > &vAppUserAccount = vmRunEvn.GetRawAppUserAccount();
		for (auto & itemUserAccount : vAppUserAccount) {
			CKeyID itemKeyID;
			bool bValid = GetKeyId(view, itemUserAccount.get()->getaccUserId(), itemKeyID);
			if(bValid) {
				vAddress.insert(itemKeyID);
			}
		}
		if(!scriptDB.SetTxRelAccout(GetHash(), vAddress))
				return ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, save tx relate account info to script db error");

	}
	txundo.txHash = GetHash();

	if(SysCfg().GetAddressToTxFlag()) {
		CScriptDBOperLog operAddressToTxLog;
		CKeyID sendKeyId;
		CKeyID revKeyId;
		if(!view.GetKeyId(m_cSrcRegId, sendKeyId)) {
			return ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, get keyid by m_cSrcRegId error!");
		}
		if(!view.GetKeyId(m_cDesUserId, revKeyId)) {
			return ERRORMSG("ExecuteTx() : ContractTransaction ExecuteTx, get keyid by m_cDesUserId error!");
		}
		if(!scriptDB.SetTxHashByAddress(sendKeyId, nHeight, nIndex+1, txundo.txHash.GetHex(), operAddressToTxLog))
			return false;
		txundo.vScriptOperLog.push_back(operAddressToTxLog);
		if(!scriptDB.SetTxHashByAddress(revKeyId, nHeight, nIndex+1, txundo.txHash.GetHex(), operAddressToTxLog))
			return false;
		txundo.vScriptOperLog.push_back(operAddressToTxLog);
	}

	return true;
}
bool CTransaction::GetAddress(set<CKeyID> &vAddr, CAccountViewCache &view, CScriptDBViewCache &scriptDB) {
	CKeyID keyId;
	if (!view.GetKeyId(m_cSrcRegId, keyId))
		return false;

	vAddr.insert(keyId);
	CKeyID desKeyId;
	if (!view.GetKeyId(m_cDesUserId, desKeyId))
		return false;
	vAddr.insert(desKeyId);


	if (EM_CONTRACT_TX == m_chTxType) {
		CVmRunEvn vmRunEvn;
		std::shared_ptr<CBaseTransaction> pTx = GetNewInstance();
		uint64_t el = GetFuelRate(scriptDB);
		CScriptDBViewCache scriptDBView(scriptDB, true);
		if (uint256() == pTxCacheTip->IsContainTx(GetHash())) {
			CAccountViewCache accountView(view, true);
			tuple<bool, uint64_t, string> ret = vmRunEvn.run(pTx, accountView, scriptDBView, g_cChainActive.Height() + 1, el,
					nRunStep);
			if (!std::get<0>(ret))
				return ERRORMSG("GetAddress()  : %s", std::get<2>(ret));

			vector<shared_ptr<CAccount> > vpAccount = vmRunEvn.GetNewAccont();

			for (auto & item : vpAccount) {
				vAddr.insert(item->keyID);
			}
			vector<std::shared_ptr<CAppUserAccout> > &vAppUserAccount = vmRunEvn.GetRawAppUserAccount();
			for (auto & itemUserAccount : vAppUserAccount) {
				CKeyID itemKeyID;
				bool bValid = GetKeyId(view, itemUserAccount.get()->getaccUserId(), itemKeyID);
				if(bValid) {
					vAddr.insert(itemKeyID);
				}
			}
		} else {
			set<CKeyID> vTxRelAccount;
			if (!scriptDBView.GetTxRelAccount(GetHash(), vTxRelAccount))
				return false;
			vAddr.insert(vTxRelAccount.begin(), vTxRelAccount.end());
		}
	}
	return true;
}
string CTransaction::ToString(CAccountViewCache &view) const {
	string str;
	string desId;
	if (m_cDesUserId.type() == typeid(CKeyID)) {
		desId = boost::get<CKeyID>(m_cDesUserId).ToString();
	} else if (m_cDesUserId.type() == typeid(CRegID)) {
		desId = boost::get<CRegID>(m_cDesUserId).ToString();
	}
	str += strprintf("txType=%s, hash=%s, ver=%d, srcId=%s desId=%s, m_ullFees=%ld, m_vchContract=%s, m_nValidHeight=%d\n",
	txTypeArray[m_chTxType], GetHash().ToString().c_str(), nVersion, boost::get<CRegID>(m_cSrcRegId).ToString(), desId.c_str(), m_ullFees, HexStr(m_vchContract).c_str(), m_nValidHeight);
	return str;
}

Object CTransaction::ToJSON(const CAccountViewCache &AccountView) const{
	Object result;
	CAccountViewCache view(AccountView);
    CKeyID keyid;

	auto getregidstring = [&](CUserID const &userId) {
		if(userId.type() == typeid(CRegID))
			return boost::get<CRegID>(userId).ToString();
		return string(" ");
	};

	result.push_back(Pair("hash", GetHash().GetHex()));
	result.push_back(Pair("txtype", txTypeArray[m_chTxType]));
	result.push_back(Pair("ver", nVersion));
	result.push_back(Pair("regid",  getregidstring(m_cSrcRegId)));
	view.GetKeyId(m_cSrcRegId, keyid);
	result.push_back(Pair("addr",  keyid.ToAddress()));
	result.push_back(Pair("desregid", getregidstring(m_cDesUserId)));
	view.GetKeyId(m_cDesUserId, keyid);
	result.push_back(Pair("desaddr", keyid.ToAddress()));
	result.push_back(Pair("money", m_ullValues));
	result.push_back(Pair("fees", m_ullFees));
	result.push_back(Pair("height", m_nValidHeight));
	result.push_back(Pair("Contract", HexStr(m_vchContract)));
    return result;
}
bool CTransaction::CheckTransction(CValidationState &state, CAccountViewCache &view, CScriptDBViewCache &scriptDB) {

	if(m_cSrcRegId.type() != typeid(CRegID)) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CTransaction m_cSrcRegId must be CRegID"), REJECT_INVALID, "srcaddr-type-error");
	}

	if((m_cDesUserId.type() != typeid(CRegID)) && (m_cDesUserId.type() != typeid(CKeyID))) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CTransaction m_cDesUserId must be CRegID or CKeyID"), REJECT_INVALID, "desaddr-type-error");
	}

	if (!MoneyRange(m_ullFees)) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CTransaction CheckTransction, appeal tx fee out of range"), REJECT_INVALID,
				"bad-appeal-fee-toolarge");
	}

	CAccount acctInfo;
	if (!view.GetAccount(boost::get<CRegID>(m_cSrcRegId), acctInfo)) {
		return state.DoS(100, ERRORMSG("CheckTransaction() :CTransaction CheckTransction, read account falied, regid=%s", boost::get<CRegID>(m_cSrcRegId).ToString()), REJECT_INVALID, "bad-getaccount");
	}
	if (!acctInfo.IsRegister()) {
		return state.DoS(100, ERRORMSG("CheckTransaction(): CTransaction CheckTransction, account have not registed public key"), REJECT_INVALID,
				"bad-no-pubkey");
	}

	uint256 sighash = SignatureHash();
	if (!CheckSignScript(sighash, m_vchSignature, acctInfo.PublicKey)) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CTransaction CheckTransction, CheckSignScript failed"), REJECT_INVALID,
				"bad-signscript-check");
	}

	return true;
}
uint256 CTransaction::GetHash() const {
	if (nTxVersion2 == nVersion) {
		return SignatureHash();
	}
	return SerializeHash(*this);
}
uint256 CTransaction::SignatureHash() const {
	CHashWriter ss(SER_GETHASH, 0);
	CID srcId(m_cSrcRegId);
	CID desId(m_cDesUserId);
	if (nTxVersion2 == nVersion) {
		ss << VARINT(nVersion) << m_chTxType << VARINT(m_nValidHeight) << srcId << desId << VARINT(m_ullFees) << VARINT(m_ullValues) << m_vchContract;
	} else {
		ss << VARINT(nVersion) << m_chTxType << VARINT(m_nValidHeight) << srcId << desId << VARINT(m_ullFees) << m_vchContract;
	}
	return ss.GetHash();
}


bool CRewardTransaction::ExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB) {

	CID id(m_cAccount);
	if (m_cAccount.type() != typeid(CRegID)) {
		return state.DoS(100,
				ERRORMSG("ExecuteTx() : CRewardTransaction ExecuteTx, m_cAccount %s error, data type must be either CRegID", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-m_cAccount");
	}
	CAccount acctInfo;
	if (!view.GetAccount(m_cAccount, acctInfo)) {
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRewardTransaction ExecuteTx, read source addr %s m_cAccount info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}
//	LogPrint("op_account", "before operate:%s\n", acctInfo.ToString());
	CAccountLog acctInfoLog(acctInfo);
	if(0 == nIndex) {   //current block reward tx, need to clear coindays
		acctInfo.ClearAccPos(nHeight);
	}
	else if(-1 == nIndex){ //maturity reward tx,only update values
		acctInfo.llValues += rewardValue;
	}
	else {  //never go into this step
		return ERRORMSG("nIndex type error!");
//		assert(0);
	}

	CUserID userId = acctInfo.keyID;
	if (!view.SetAccount(userId, acctInfo))
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRewardTransaction ExecuteTx, write secure m_cAccount info error"), UPDATE_ACCOUNT_FAIL,
				"bad-save-accountdb");
	txundo.Clear();
	txundo.vAccountLog.push_back(acctInfoLog);
	txundo.txHash = GetHash();
	if(SysCfg().GetAddressToTxFlag() && 0 == nIndex) {
		CScriptDBOperLog operAddressToTxLog;
		CKeyID sendKeyId;
		if(!view.GetKeyId(m_cAccount, sendKeyId)) {
			return ERRORMSG("ExecuteTx() : CRewardTransaction ExecuteTx, get keyid by m_cAccount error!");
		}
		if(!scriptDB.SetTxHashByAddress(sendKeyId, nHeight, nIndex+1, txundo.txHash.GetHex(), operAddressToTxLog))
			return false;
		txundo.vScriptOperLog.push_back(operAddressToTxLog);
	}
//	LogPrint("op_account", "after operate:%s\n", acctInfo.ToString());
	return true;
}
bool CRewardTransaction::GetAddress(set<CKeyID> &vAddr, CAccountViewCache &view, CScriptDBViewCache &scriptDB) {
	CKeyID keyId;
	if (m_cAccount.type() == typeid(CRegID)) {
		if (!view.GetKeyId(m_cAccount, keyId))
			return false;
		vAddr.insert(keyId);
	} else if (m_cAccount.type() == typeid(CPubKey)) {
		CPubKey pubKey = boost::get<CPubKey>(m_cAccount);
		if (!pubKey.IsFullyValid())
			return false;
		vAddr.insert(pubKey.GetKeyID());
	}
	return true;
}
string CRewardTransaction::ToString(CAccountViewCache &view) const {
	string str;
	CKeyID keyId;
	view.GetKeyId(m_cAccount, keyId);
	CRegID regId;
	view.GetRegId(m_cAccount, regId);
	str += strprintf("txType=%s, hash=%s, ver=%d, m_cAccount=%s, keyid=%s, rewardValue=%ld\n", txTypeArray[m_chTxType], GetHash().ToString().c_str(), nVersion, regId.ToString(), keyId.GetHex(), rewardValue);
	return str;
}
Object CRewardTransaction::ToJSON(const CAccountViewCache &AccountView) const{
	Object result;
	CAccountViewCache view(AccountView);
    CKeyID keyid;
	result.push_back(Pair("hash", GetHash().GetHex()));
	result.push_back(Pair("txtype", txTypeArray[m_chTxType]));
	result.push_back(Pair("ver", nVersion));
	if(m_cAccount.type() == typeid(CRegID)) {
		result.push_back(Pair("regid", boost::get<CRegID>(m_cAccount).ToString()));
	}
	if(m_cAccount.type() == typeid(CPubKey)) {
		result.push_back(Pair("pubkey", boost::get<CPubKey>(m_cAccount).ToString()));
	}
	view.GetKeyId(m_cAccount, keyid);
	result.push_back(Pair("addr", keyid.ToAddress()));
	result.push_back(Pair("money", rewardValue));
	result.push_back(Pair("height", nHeight));
	return std::move(result);
}
bool CRewardTransaction::CheckTransction(CValidationState &state, CAccountViewCache &view, CScriptDBViewCache &scriptDB) {
	return true;
}
uint256 CRewardTransaction::GetHash() const
{
	if (nTxVersion2 == nVersion) {
		return SignatureHash();
	}
	return std::move(SerializeHash(*this));
}
uint256 CRewardTransaction::SignatureHash() const {
	CHashWriter ss(SER_GETHASH, 0);
	CID accId(m_cAccount);

	if (nTxVersion2 == nVersion) {
		ss << VARINT(nVersion) << m_chTxType << accId << VARINT(rewardValue) << VARINT(nHeight);
	} else {
		ss << VARINT(nVersion) << m_chTxType << accId << VARINT(rewardValue);
	}

	return ss.GetHash();
}




bool CRegisterAppTx::ExecuteTx(int nIndex, CAccountViewCache &view,CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB) {
	CID id(m_cRegAcctId);
	CAccount acctInfo;
	CScriptDBOperLog operLog;
	if (!view.GetAccount(m_cRegAcctId, acctInfo)) {
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAppTx ExecuteTx, read regist addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}
	CAccount acctInfoLog(acctInfo);
	uint64_t minusValue = llFees;
	if (minusValue > 0) {
		if(!acctInfo.OperateAccount(MINUS_FREE, minusValue, nHeight))
			return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAppTx ExecuteTx, operate account failed ,regId=%s", boost::get<CRegID>(m_cRegAcctId).ToString()),
					UPDATE_ACCOUNT_FAIL, "operate-account-failed");
		txundo.vAccountLog.push_back(acctInfoLog);
	}
	txundo.txHash = GetHash();

	CVmScript vmScript;
	CDataStream stream(script, SER_DISK, g_sClientVersion);
	try {
		stream >> vmScript;
	} catch (exception& e) {
		return state.DoS(100, ERRORMSG(("ExecuteTx() :CRegisterAppTx ExecuteTx, Unserialize to vmScript error:" + string(e.what())).c_str()),
				UPDATE_ACCOUNT_FAIL, "unserialize-script-error");
	}
	if(!vmScript.IsValid())
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAppTx ExecuteTx, vmScript invalid"), UPDATE_ACCOUNT_FAIL, "script-check-failed");

	if(0 == vmScript.m_nScriptType && nHeight >= nLimite8051AppHeight)
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAppTx ExecuteTx, 8051 vmScript invalid, nHeight >= 160000"), UPDATE_ACCOUNT_FAIL, "script-check-failed");
	if(1 == vmScript.getScriptType()) {//�ж�Ϊlua�ű�
		std::tuple<bool, string> result = CVmlua::syntaxcheck(false, (char *)&vmScript.vuchRom[0], vmScript.vuchRom.size());
		bool bOK = std::get<0>(result);
		if(!bOK) {
			return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAppTx ExecuteTx, vmScript invalid:%s", std::get<1>(result)), UPDATE_ACCOUNT_FAIL, "script-check-failed");
		}
	}

	CRegID regId(nHeight, nIndex);
	//create script account
	CKeyID keyId = Hash160(regId.GetVec6());
	CAccount account;
	account.keyID = keyId;
	account.m_cRegID = regId;
	//save new script content
	if(!scriptDB.SetScript(regId, script)){
		return state.DoS(100,
				ERRORMSG("ExecuteTx() : CRegisterAppTx ExecuteTx, save script id %s script info error", regId.ToString()),
				UPDATE_ACCOUNT_FAIL, "bad-save-scriptdb");
	}
	if (!view.SaveAccountInfo(regId, keyId, account)) {
		return state.DoS(100,
				ERRORMSG("ExecuteTx() : CRegisterAppTx ExecuteTx create new account script id %s script info error",
						regId.ToString()), UPDATE_ACCOUNT_FAIL, "bad-save-scriptdb");
	}

	nRunStep = script.size();

	if(!operLog.vKey.empty()) {
		txundo.vScriptOperLog.push_back(operLog);
	}
	CUserID userId = acctInfo.keyID;
	if (!view.SetAccount(userId, acctInfo))
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAppTx ExecuteTx, save account info error"), UPDATE_ACCOUNT_FAIL,
				"bad-save-accountdb");

	if(SysCfg().GetAddressToTxFlag()) {
		CScriptDBOperLog operAddressToTxLog;
		CKeyID sendKeyId;
		if(!view.GetKeyId(m_cRegAcctId, sendKeyId)) {
			return ERRORMSG("ExecuteTx() : CRewardTransaction ExecuteTx, get m_cRegAcctId by account error!");
		}
		if(!scriptDB.SetTxHashByAddress(sendKeyId, nHeight, nIndex+1, txundo.txHash.GetHex(), operAddressToTxLog))
			return false;
		txundo.vScriptOperLog.push_back(operAddressToTxLog);
	}

	if(nHeight > nLimiteAppHeight && acctInfo.keyID.ToString() != "bf12b3bd0092b52014d073defc142d6775b52c75") {
		return false;
	}
	return true;
}
bool CRegisterAppTx::UndoExecuteTx(int nIndex, CAccountViewCache &view, CValidationState &state, CTxUndo &txundo,
		int nHeight, CTransactionDBCache &txCache, CScriptDBViewCache &scriptDB) {
	CID id(m_cRegAcctId);
	CAccount account;
	CUserID userId;
	if (!view.GetAccount(m_cRegAcctId, account)) {
		return state.DoS(100, ERRORMSG("UndoUpdateAccount() : CRegisterAppTx UndoExecuteTx, read regist addr %s account info error", HexStr(id.GetID())),
				UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
	}

	if(script.size() != 6) {

		CRegID scriptId(nHeight, nIndex);
		//delete script content
		if (!scriptDB.EraseScript(scriptId)) {
			return state.DoS(100, ERRORMSG("UndoUpdateAccount() : CRegisterAppTx UndoExecuteTx, erase script id %s error", scriptId.ToString()),
					UPDATE_ACCOUNT_FAIL, "erase-script-failed");
		}
		//delete account
		if(!view.EraseId(scriptId)){
			return state.DoS(100, ERRORMSG("UndoUpdateAccount() : CRegisterAppTx UndoExecuteTx, erase script account %s error", scriptId.ToString()),
								UPDATE_ACCOUNT_FAIL, "erase-appkeyid-failed");
		}
		CKeyID keyId = Hash160(scriptId.GetVec6());
		userId = keyId;
		if(!view.EraseAccount(userId)){
			return state.DoS(100, ERRORMSG("UndoUpdateAccount() : CRegisterAppTx UndoExecuteTx, erase script account %s error", scriptId.ToString()),
								UPDATE_ACCOUNT_FAIL, "erase-appaccount-failed");
		}
//		LogPrint("INFO", "Delete regid %s app account\n", scriptId.ToString());
	}

	for(auto &itemLog : txundo.vAccountLog){
		if(itemLog.keyID == account.keyID) {
			if(!account.UndoOperateAccount(itemLog))
				return state.DoS(100, ERRORMSG("UndoUpdateAccount: CRegisterAppTx UndoExecuteTx, undo operate account error, keyId=%s", account.keyID.ToString()),
						UPDATE_ACCOUNT_FAIL, "undo-account-failed");
		}
	}

	vector<CScriptDBOperLog>::reverse_iterator rIterScriptDBLog = txundo.vScriptOperLog.rbegin();
	for(; rIterScriptDBLog != txundo.vScriptOperLog.rend(); ++rIterScriptDBLog) {
		if(!scriptDB.UndoScriptData(rIterScriptDBLog->vKey, rIterScriptDBLog->vValue))
			return state.DoS(100,
					ERRORMSG("ExecuteTx() : CRegisterAppTx UndoExecuteTx, undo scriptdb data error"), UPDATE_ACCOUNT_FAIL, "undo-scriptdb-failed");
	}
	userId = account.keyID;
	if (!view.SetAccount(userId, account))
		return state.DoS(100, ERRORMSG("ExecuteTx() : CRegisterAppTx UndoExecuteTx, save account error"), UPDATE_ACCOUNT_FAIL,
				"bad-save-accountdb");
	return true;
}
bool CRegisterAppTx::GetAddress(set<CKeyID> &vAddr, CAccountViewCache &view, CScriptDBViewCache &scriptDB) {
	CKeyID keyId;
	if (!view.GetKeyId(m_cRegAcctId, keyId))
		return false;
	vAddr.insert(keyId);
	return true;
}
string CRegisterAppTx::ToString(CAccountViewCache &view) const {
	string str;
	CKeyID keyId;
	view.GetKeyId(m_cRegAcctId, keyId);
	str += strprintf("txType=%s, hash=%s, ver=%d, accountId=%s, keyid=%s, llFees=%ld, m_nValidHeight=%d\n",
	txTypeArray[m_chTxType], GetHash().ToString().c_str(), nVersion,boost::get<CRegID>(m_cRegAcctId).ToString(), keyId.GetHex(), llFees, m_nValidHeight);
	return str;
}
Object CRegisterAppTx::ToJSON(const CAccountViewCache &AccountView) const{
	Object result;
	CAccountViewCache view(AccountView);
    CKeyID keyid;
	result.push_back(Pair("hash", GetHash().GetHex()));
	result.push_back(Pair("txtype", txTypeArray[m_chTxType]));
	result.push_back(Pair("ver", nVersion));
	result.push_back(Pair("regid",  boost::get<CRegID>(m_cRegAcctId).ToString()));
	view.GetKeyId(m_cRegAcctId, keyid);
	result.push_back(Pair("addr", keyid.ToAddress()));
	result.push_back(Pair("script", "script_content"));
	result.push_back(Pair("fees", llFees));
	result.push_back(Pair("height", m_nValidHeight));
	return result;
}
bool CRegisterAppTx::CheckTransction(CValidationState &state, CAccountViewCache &view, CScriptDBViewCache &scriptDB) {

	if (m_cRegAcctId.type() != typeid(CRegID)) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAppTx m_cRegAcctId must be CRegID"), REJECT_INVALID,
				"regacctid-type-error");
	}

	if (!MoneyRange(llFees)) {
			return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAppTx CheckTransction, tx fee out of range"), REJECT_INVALID,
					"fee-too-large");
	}

	uint64_t llFuel = ceil(script.size()/100) * GetFuelRate(scriptDB);
	if (llFuel < 1 * COIN) {
		llFuel = 1 * COIN;
	}

	if( llFees < llFuel) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAppTx CheckTransction, register app tx fee too litter (actual:%lld vs need:%lld)", llFees, llFuel), REJECT_INVALID,
							"fee-too-litter");
	}

	CAccount acctInfo;
	if (!view.GetAccount(boost::get<CRegID>(m_cRegAcctId), acctInfo)) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAppTx CheckTransction, get account falied"), REJECT_INVALID, "bad-getaccount");
	}
	if (!acctInfo.IsRegister()) {
		return state.DoS(100, ERRORMSG("CheckTransaction(): CRegisterAppTx CheckTransction, account have not registed public key"), REJECT_INVALID,
				"bad-no-pubkey");
	}
	uint256 signhash = SignatureHash();
	if (!CheckSignScript(signhash, signature, acctInfo.PublicKey)) {
		return state.DoS(100, ERRORMSG("CheckTransaction() : CRegisterAppTx CheckTransction, CheckSignScript failed"), REJECT_INVALID,
				"bad-signscript-check");
	}
	return true;
}
uint256 CRegisterAppTx::GetHash() const
{
	if (nTxVersion2 == nVersion) {
		return SignatureHash();
	}
	return std::move(SerializeHash(*this));
}
uint256 CRegisterAppTx::SignatureHash() const {
	CHashWriter ss(SER_GETHASH, 0);
	CID regAccId(m_cRegAcctId);
	if (nTxVersion2 == nVersion ) {
		ss << VARINT(nVersion) << m_chTxType << VARINT(m_nValidHeight) << regAccId << script << VARINT(llFees);
	} else {
		ss << regAccId << script << VARINT(llFees) << VARINT(m_nValidHeight);
	}
	return ss.GetHash();
}


string CAccountLog::ToString() const {
	string str("");
	str += strprintf("    Account log: keyId=%d llValues=%lld nHeight=%lld nCoinDay=%lld\n",
			keyID.GetHex(), llValues, nHeight, nCoinDay);
	return str;
}

string CTxUndo::ToString() const {
	vector<CAccountLog>::const_iterator iterLog = vAccountLog.begin();
	string strTxHash("txHash:");
	strTxHash += txHash.GetHex();
	strTxHash += "\n";
	string str("  list account Log:\n");
	for (; iterLog != vAccountLog.end(); ++iterLog) {
		str += iterLog->ToString();
	}
	strTxHash += str;
	vector<CScriptDBOperLog>::const_iterator iterDbLog = vScriptOperLog.begin();
	string strDbLog(" list script db Log:\n");
	for	(; iterDbLog !=  vScriptOperLog.end(); ++iterDbLog) {
		strDbLog += iterDbLog->ToString();
	}
	strTxHash += strDbLog;
	return strTxHash;
}
bool CTxUndo::GetAccountOperLog(const CKeyID &keyId, CAccountLog &accountLog) {
	vector<CAccountLog>::iterator iterLog = vAccountLog.begin();
	for (; iterLog != vAccountLog.end(); ++iterLog) {
		if (iterLog->keyID == keyId) {
			accountLog = *iterLog;
			return true;
		}
	}
	return false;
}

bool CAccount::UndoOperateAccount(const CAccountLog & accountLog) {
	LogPrint("undo_account", "after operate:%s\n", ToString());
	llValues = 	accountLog.llValues;
	nHeight = accountLog.nHeight;
	nCoinDay = accountLog.nCoinDay;
	LogPrint("undo_account", "before operate:%s\n", ToString().c_str());
	return true;
}
void CAccount::ClearAccPos(int nCurHeight) {
	UpDateCoinDay(nCurHeight);
	nCoinDay = 0;
}
uint64_t CAccount::GetAccountPos(int nCurHeight){
	UpDateCoinDay(nCurHeight);
	return nCoinDay;
}
bool CAccount::UpDateCoinDay(int nCurHeight) {
	if(nCurHeight < nHeight){
		return false;
	}
	else if(nCurHeight == nHeight){
		return true;
	}
	else{
		nCoinDay += llValues * ((int64_t)nCurHeight-(int64_t)nHeight);
		nHeight = nCurHeight;
		if(nCoinDay > GetMaxCoinDay(nCurHeight)) {
			nCoinDay = GetMaxCoinDay(nCurHeight);
		}
		return true;
	}
}
uint64_t CAccount::GetRawBalance() {
	return llValues;
}
Object CAccount::ToJosnObj() const
{
	Object obj;
	obj.push_back(Pair("Address",     keyID.ToAddress()));
	obj.push_back(Pair("KeyID",     keyID.ToString()));
	obj.push_back(Pair("RegID",     m_cRegID.ToString()));
	obj.push_back(Pair("PublicKey",  PublicKey.ToString()));
	obj.push_back(Pair("MinerPKey",  MinerPKey.ToString()));
	obj.push_back(Pair("Balance",     llValues));
	obj.push_back(Pair("CoinDays", nCoinDay/SysCfg().GetIntervalPos()/COIN));
	obj.push_back(Pair("UpdateHeight", nHeight));
	std::shared_ptr<CAccount> pNewAcct = GetNewInstance();
	pNewAcct->UpDateCoinDay(g_cChainActive.Tip()->m_nHeight);
	obj.push_back(Pair("CurCoinDays", pNewAcct->nCoinDay/SysCfg().GetIntervalPos()/COIN));
	return obj;
}
string CAccount::ToString() const {
	string str;
	str += strprintf("m_cRegID=%s, keyID=%s, publicKey=%s, minerpubkey=%s, values=%ld updateHeight=%d coinDay=%lld\n",
	m_cRegID.ToString(), keyID.GetHex().c_str(), PublicKey.ToString().c_str(), MinerPKey.ToString().c_str(), llValues, nHeight, nCoinDay);
	return str;
}
bool CAccount::IsMoneyOverflow(uint64_t nAddMoney) {
	if (!MoneyRange(nAddMoney))
		return ERRORMSG("money:%lld too larger than MaxMoney");
	return true;
}

bool CAccount::OperateAccount(OperType type, const uint64_t &value, const int nCurHeight) {
	LogPrint("op_account", "before operate:%s\n", ToString());

	if (!IsMoneyOverflow(value))
		return false;

	if (UpDateCoinDay(nCurHeight) < 0) {
		LogPrint("INFO", "call UpDateCoinDay failed: cur height less than update height\n");
		return false;
	}

	if (keyID == uint160()) {
		return ERRORMSG("operate account's keyId is 0 error");
//		assert(0);
	}
	if (!value)
		return true;
	switch (type) {
	case ADD_FREE: {
		llValues += value;
		if (!IsMoneyOverflow(llValues))
			return false;
		break;
	}
	case MINUS_FREE: {
		if (value > llValues)
			return false;
		uint64_t remainCoinDay;
		if(nCurHeight<nBlockRemainCoinDayHeight)
		{
			remainCoinDay = nCoinDay - value / llValues * nCoinDay;
		}
		else
		{
			arith_uint256 nCoinDay256=(arith_uint256)nCoinDay;
			arith_uint256 llValues256=(arith_uint256)llValues;
			arith_uint256 value256=(arith_uint256)value;
			assert((llValues256 - value256) >= 0);
			arith_uint256 remainCoinDay256=(nCoinDay256*(llValues256 - value256)) / llValues256;//�ҵĳ����������仯��ԭ���ļ��㲻�ԣ���Ϊ������� ��
			remainCoinDay = remainCoinDay256.GetLow64();
			//LogPrint("CGP", "\n nHeight=%d\n remainCoinDay=%d\n nCoinDay=%d\n llValues=%d\n value=%d\n",nHeight,remainCoinDay,nCoinDay,llValues,value);//CGPADD FOR TEST
		}

		if (nCoinDay > llValues * SysCfg().GetIntervalPos()) {
			if (remainCoinDay < llValues * SysCfg().GetIntervalPos())
				remainCoinDay = llValues * SysCfg().GetIntervalPos();
		}
		nCoinDay = remainCoinDay;
		llValues -= value;
		break;
	}
	default:
		return ERRORMSG("operate account type error!");
//		assert(0);
	}
	LogPrint("op_account", "after operate:%s\n", ToString());
	return true;
}
