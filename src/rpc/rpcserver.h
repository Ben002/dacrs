// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The DACRS developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DACRS_RPC_RPCSERVER_H_
#define DACRS_RPC_RPCSERVER_H_ 1

#include "uint256.h"
#include "rpcprotocol.h"

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"
using namespace std;
class CBlockIndex;

/* Start RPC threads */
void StartRPCThreads();
/* Alternative to StartRPCThreads for the GUI, when no server is
 * used. The RPC thread in this case is only used to handle timeouts.
 * If real RPC threads have already been started this is a no-op.
 */
void StartDummyRPCThread();
/* Stop RPC threads */
void StopRPCThreads();

/*
  Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
  the right number of arguments are passed, just that any passed are the correct type.
  Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
*/
void RPCTypeCheck(const json_spirit::Array& params, const list<json_spirit::Value_type>& typesExpected,
		bool bAllowNull = false);
/*
  Check for expected keys/value types in an Object.
  Use like: RPCTypeCheck(object, boost::assign::map_list_of("name", str_type)("value", int_type));
*/
void RPCTypeCheck(const json_spirit::Object& o, const map<string, json_spirit::Value_type>& typesExpected,
		bool bAllowNull = false);

/*
  Run func nSeconds from now. Uses boost deadline timers.
  Overrides previous timer <name> (if any).
 */
void RPCRunLater(const string& strName, boost::function<void(void)> func, int64_t llSeconds);

typedef json_spirit::Value(*rpcfn_type)(const json_spirit::Array& params, bool bHelp);

class CRPCCommand {
 public:
	string m_strName;
	rpcfn_type m_Actor;
	bool m_bOkSafeMode;
	bool m_bThreadSafe;
	bool m_bReqWallet;
};

/**
 * DACRS RPC command dispatcher.
 */
class CRPCTable {
 public:
	CRPCTable();
	const CRPCCommand* operator[](string strName) const;
	string help(string strCommand) const;

	/**
	 * Execute a method.
	 * @param method   Method to execute
	 * @param params   Array of arguments (JSON objects)
	 * @returns Result of the call.
	 * @throws an exception (json_spirit::Value) when an error happens.
	 */
	json_spirit::Value execute(const string &strMethod, const json_spirit::Array &params) const;

 private:
	map<string, const CRPCCommand*> m_mapCommands;
};

extern const CRPCTable g_TableRPC;

//
// Utilities: convert hex-encoded Values
// (throws error if not hex).
//
extern uint256 ParseHashV(const json_spirit::Value& v, string strName);
extern uint256 ParseHashO(const json_spirit::Object& o, string strKey);
extern vector<unsigned char> ParseHexV(const json_spirit::Value& v, string strName);
extern vector<unsigned char> ParseHexO(const json_spirit::Object& o, string strKey);

extern void InitRPCMining();
extern void ShutdownRPCMining();

extern int64_t g_llWalletUnlockTime;
extern int64_t AmountToRawValue(const json_spirit::Value& value);
extern json_spirit::Value ValueFromAmount(int64_t llAmount);
extern double GetDifficulty(const CBlockIndex* pcBlockIndex = NULL);
extern string HexBits(unsigned int nBits);
extern string HelpRequiringPassphrase();
extern string HelpExampleCli(string strMethodName, string args);
extern string HelpExampleRpc(string strMethodName, string args);

extern void EnsureWalletIsUnlocked();

extern json_spirit::Value getconnectioncount(const json_spirit::Array& params, bool bHelp); // in rpcnet.cpp
extern json_spirit::Value getpeerinfo(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value ping(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value addnode(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getaddednodeinfo(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getnettotals(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getdacrsstate(const json_spirit::Array& params, bool bHelp);

extern json_spirit::Value dumpprivkey(const json_spirit::Array& params, bool bHelp); // in rpcdump.cpp
extern json_spirit::Value importprivkey(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value dumpwallet(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value importwallet(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value dropprivkey(const json_spirit::Array& params, bool bHelp);

//extern json_spirit::Value getgenerate(const json_spirit::Array& params, bool bHelp); // in rpcmining.cpp
extern json_spirit::Value setgenerate(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getnetworkhashps(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value gethashespersec(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getmininginfo(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getwork(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getblocktemplate(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value submitblock(const json_spirit::Array& params, bool bHelp);

extern json_spirit::Value getnewaddress(const json_spirit::Array& params, bool bHelp); // in rpcwallet.cpp
extern json_spirit::Value getaccount(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value verifymessage(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getbalance(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value addmultisigaddress(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value islocked(const json_spirit::Array& params, bool bHelp);

extern json_spirit::Value backupwallet(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value walletpassphrase(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value walletpassphrasechange(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value walletlock(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value encryptwallet(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getinfo(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getwalletinfo(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getblockchaininfo(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getnetworkinfo(const json_spirit::Array& params, bool bHelp);

extern json_spirit::Value getrawtransaction(const json_spirit::Array& params, bool bHelp); // in rcprawtransaction.cpp
extern json_spirit::Value createrawtransaction(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value decoderawtransaction(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value signrawtransaction(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value sendrawtransaction(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value signmessage(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value notionalpoolingbalance(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value dispersebalance(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value notionalpoolingasset(const json_spirit:: Array& params, bool bHelp);
extern json_spirit::Value getassets(const json_spirit:: Array& params, bool bHelp);
extern json_spirit::Value sendtoaddress(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value sendtoaddresswithfee(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value sendtoaddressraw(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value registaccounttxraw(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value submittx(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value createcontracttxraw(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value createfreezetxraw(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value registerscripttxraw(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value sigstr(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value printblokdbinfo(const json_spirit::Array& params, bool bHelp);


extern json_spirit::Value getblockcount(const json_spirit::Array& params, bool bHelp); // in rpcblockchain.cpp
extern json_spirit::Value getbestblockhash(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getdifficulty(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value settxfee(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getrawmempool(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getblockhash(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getblock(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value gettxoutsetinfo(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value verifychain(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value listsetblockindexvalid(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value getscriptid(const json_spirit::Array& params, bool bHelp);
extern json_spirit::Value listcheckpoint(const json_spirit::Array& params, bool bHelp);
#endif
