#ifndef PO_BaseTp_Group_H
#define PO_BaseTp_Group_H

#include "dbparam/PO_Common.h"
#include "FieldsXX.h"

class CondBase;
class PO_BaseTp_Group
{
  public:
    bool getProperty(PO_Property &property);

    bool convert(const StringVector &argname);

    PO_BaseTp_Group(){}

    PO_BaseTp_Group(const PO_BaseTp_Group& right);

    void operator=(const PO_BaseTp_Group& right);

    friend int operator==(const PO_BaseTp_Group &left,const PO_BaseTp_Group& right);

    friend int operator<(const PO_BaseTp_Group &left,const PO_BaseTp_Group& right);

	friend ostream& operator<<(ostream& os,const PO_BaseTp_Group& po);

    struct
    {
    	T_TP_ID        m_tpId;
  		T_DATE_TIME    m_beginTime;
  		T_DATE_TIME    m_endTime;
  		T_BIZ_TYPE     m_bizType;
  		T_AREA_CODE    m_homeAreaCode;
  		T_CALL_TYPE    m_callType;
  		T_ROAM_TYPE    m_roamType;
  		T_SERV_TYPE    m_servType;
		  T_CARRIER_TYPE m_carrierType;
		  T_RULE_ID      m_ruleId;
		  CondBase      *m_condRule; //条件指针
		  T_FLAG 				 m_effectBizTag;//作用事件标志： 1: U文件触发  其他：话单U文件都触发 add by xuf 2009-11-7 10:06:49
    }m_record;

  private:
	static PO_Property m_property;

};



#endif /* PO_BaseTp_Group_H */
