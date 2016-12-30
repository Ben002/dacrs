/*
 * lashou_tests.h
 *
 *  Created on: 2016��12��29��
 *      Author: ranger.shi
 */

#ifndef LASHOU_TESTS_H_
#define LASHOU_TESTS_H_

#include "cycle_test_base.h"
#include "../test/systestbase.h"
#include "../rpc/rpcclient.h"
#include "tx.h"

using namespace std;
using namespace boost;
using namespace json_spirit;

#if 1

#define TX_CONFIG   	 0x01//--������Ϣ���������ò����ʹ洢һЩȫ�ֱ�����
#define TX_MODIFIED 	 0X02//--�޸�������Ϣ
#define TX_REGISTER 	 0X03//--ע����Ϣ
#define TX_RECHARGE 	 0x04//--��ֵ
#define TX_WITHDRAW 	 0x05//--��������
#define TX_CLAIM_APPLY 	 0X06//--����������Ϣ
#define TX_CLAIM_OPERATE 0X07//--�������
#define TX_IMPORT_DATA   0X08//--�����û�����
#define TX_MODIFIED_TIME 0X09//--�޸ļ���ʱ��

typedef struct
{
	unsigned char type;    //!<��������
	uint32_t  WatchDay;//!�۲�������
	uint32_t  MinBalance;//!��������ֵ
	char SuperAcc[35];//!�����û�
	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			READWRITE(WatchDay);
			READWRITE(MinBalance);
			for(int i = 0; i < 35; i++)
			{
				READWRITE(SuperAcc[i]);
			}
	)
}CONFIG_ST;  //!<ע��������Ϣ

//
typedef struct {
	unsigned char type;            //!<��������
	char  ModityAcc[35];//!<���׽��

	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			for(int i = 0; i < 35; i++)
			{
				READWRITE(ModityAcc[i]);
			}
	)
}MODIFIED_ST;//!<�޸��ʻ���Ϣ


typedef struct
{
	unsigned char type;    //!<��������
	uint32_t  RegMoney;//!ע����
	char UserID[35];//!�û�ID
	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			READWRITE(RegMoney);
			for(int i = 0; i < 35; i++)
			{
				READWRITE(UserID[i]);
			}
	)
}REGISTER_ST;  //!<ע���û���Ϣ


typedef struct
{
	unsigned char type;    //!<��������
	uint32_t  Money;//!��ֵ���
	char UserID[35];//!�û�ID
	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			READWRITE(Money);
			for(int i = 0; i < 35; i++)
			{
				READWRITE(UserID[i]);
			}
	)
}RECHARGE_ST;  //!<��ֵ������


typedef struct
{
	unsigned char type;    //!<��������
	char UserID[34];//!�û�ID
	char ApplyHash[35];//!�����HASH
	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			for(int i = 0; i < 34; i++)
			{
				READWRITE(UserID[i]);
			}

			for(int i = 0; i < 35; i++)
			{
				READWRITE(ApplyHash[i]);
			}

	)
}APPLY_ST;  //!<��������



typedef struct
{
	unsigned char type;    //!<��������
	uint32_t  Money;//!��ֵ���
	char UserID[34];//!�û�ID
	uint32_t  Number;//!����
	char ApplyHash[34*3+1];//!�����HASH
	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			READWRITE(Money);
			for(int i = 0; i < 34; i++)
			{
				READWRITE(UserID[i]);
			}
			READWRITE(Number);
			for(int i = 0; i < (34*3+1); i++)
			{
				READWRITE(ApplyHash[i]);
			}

	)
}CLAIMS_ST;  //!<�������

#define IMPORT_DATA_NNNN	90
typedef struct
{
	unsigned char type; //!<��������
	uint32_t  Number;	//!��ֵ���
	struct
	{//!<�������ݽṹ
		char UserID[34];//!�û�ID
		uint32_t ImportMoney;//!ע����
		uint32_t ImportHight;//!ע��߶�
	} ImportDataSt[IMPORT_DATA_NNNN];
	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			READWRITE(Number);
			for(int i = 0; i < IMPORT_DATA_NNNN; i++)
			{
				for(int j = 0; j < 34; j++)
				{
					READWRITE(ImportDataSt[i].UserID[j]);
				}
				READWRITE(ImportDataSt[i].ImportMoney);
				READWRITE(ImportDataSt[i].ImportHight);
			}
	)
}IMPORT_ST;  //!<�������




//======================================================================
//======================================================================
//======================================================================

enum GETDAWEL{
	TX_REGID = 0x01,
	TX_BASE58 = 0x02,
};

typedef struct {
	unsigned char type;            //!<��������
	uint64_t maxMoneyByTime;       //!<ÿ���޶�
	uint64_t maxMoneyByDay;        //!<ÿ���޶�
	char  address[35];             //!<��ע˵�� �ַ�����\0���������Ȳ����0
	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			READWRITE(maxMoneyByTime);
			READWRITE(maxMoneyByDay);
			for(int i = 0; i < 35; i++)
			{
				READWRITE(address[i]);
			}
	)
}COMPANY_CONFIG;  //!<ע����ҵ������Ϣ
typedef struct {
	unsigned char type;            //!<��������
	uint64_t maxMoneyByTime;       //!<ÿ���޶�
	uint64_t maxMoneyByDay;        //!<ÿ���޶�
//	char  address[35];             //!<��ע˵�� �ַ�����\0���������Ȳ����0
	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			READWRITE(maxMoneyByTime);
			READWRITE(maxMoneyByDay);
//			for(int i = 0; i < 220; i++)
//			{
//				READWRITE(address[i]);
//			}
	)
}COMPANY_CONFIG_MODIFY;  //!<�޸���ҵ������Ϣ

typedef struct {
	unsigned char type;            //!<��������
//	uint64_t moneyM;                   //!<���׽��

	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
//			READWRITE(moneyM);
	)
}COMPANY_RECHARGE;                  //!<��ҵ������


typedef struct {
	unsigned char type;            //!<��������
	char  address[35];             //!<��ע˵�� �ַ�����\0���������Ȳ����0
	uint64_t moneyM;               //!<���׽��

	IMPLEMENT_SERIALIZE
	(
			READWRITE(type);
			for(int i = 0; i < 35; i++)
			{
				READWRITE(address[i]);
			}
			READWRITE(moneyM);
	)
} COMPANY_WITHDRAW;


class CLashouTest: public CycleTestBase {
	int nNum;
	int nStep;
	string strTxHash;
	string strAppRegId;//ע��Ӧ�ú��Id
public:
	CLashouTest();
	~CLashouTest(){};
	virtual emTEST_STATE Run() ;
	bool RegistScript();

	bool Config(void);
	bool Modify(void);
	bool Register(void);
	bool Recharge(void);
	bool Withdraw(void);
	bool ApplyForClaims(void);
	bool ClaimsOperate(void);
	bool ImportDate(void);
	bool ImportDateNN(void);
	bool CodeTest(void);
};
#endif





#endif /* LASHOU_TESTS_H_ */
