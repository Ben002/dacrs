/*
 * VmScript.h
 *
 *  Created on: Sep 2, 2014
 *      Author: ranger.shi
 */

#ifndef VMSCRIPT_H_
#define VMSCRIPT_H_

#include "serialize.h"
using namespace std;

/**
 * @brief Load script binary code class
 */
class CVmScript {

public:
	
	vector<unsigned char> vuchRom;      		//!< Binary code
	vector<unsigned char> vuchScriptExplain;	// !<explain the binary code action
	int  m_nScriptType = 0;                    //!<�ű������� 0:8051,1:lua

public:
	/**
	 * @brief
	 * @return
	 */
	bool IsValid() {
		///Binary code'size less 64k
		if ((vuchRom.size() > 64 * 1024) || (vuchRom.size() <= 0)) {
			return false;
		}
		if (vuchRom[0] != 0x02) {
			if (!memcmp(&vuchRom[0], "mylib = require", strlen("mylib = require"))) {
				m_nScriptType = 1;                    //lua�ű�
				return true;                    //lua�ű���ֱ�ӷ���
			} else {
				return false;
			}
		} else {
			m_nScriptType = 0;                    //8051�ű�
		}
		//!<ָ���汾��SDK���ϣ���ȥУ�� �˻�ƽ�⿪�ص�ȡֵ
		if (memcmp(&vuchRom[0x0004], "\x00\x02\x02", 3) >= 0) {
			if (!((vuchRom[0x0014] == 0x00) || (vuchRom[0x0014] == 0x01))) {
				return false;
			}
		}
		return true;
	}

	bool IsCheckAccount(void) {
		if (m_nScriptType) {
			return false;                    //lua�ű���ֱ�ӷ���(�ص��˻�ƽ��)
		}

		//!<ָ���汾��SDK���ϣ���ȥ��ȡ �˻�ƽ�⿪�ص�ȡֵ
		if (memcmp(&vuchRom[0x0004], "\x00\x02\x02", 3) >= 0) {
			if (vuchRom[0x0014] == 0x01) {
				return true;
			}
		}
		return false;
	}
	CVmScript();
	int getScriptType() {
		return m_nScriptType;
	}
	IMPLEMENT_SERIALIZE
	(
			READWRITE(vuchRom);
			READWRITE(vuchScriptExplain);
	)
	virtual ~CVmScript();
};

#endif /* VMSCRIPT_H_ */
