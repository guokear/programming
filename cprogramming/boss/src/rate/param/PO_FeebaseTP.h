#ifndef PO_FEEBASETP_H
#define PO_FEEBASETP_H

#include "dbparam/PO_Common.h"
#include "FieldsXX.h"

const int BASETP_FACTORS_COUNT = 12;

class CondBase;
class FeebaseTP 
{   
  public:
	T_FEE_ID          m_tpId;
	T_PRI             m_priority;
	T_FEE_FIELD       m_feeField;

  T_BIZ_TYPE        m_bizType;
	T_ATTRIBUTE_VALUE m_factors[BASETP_FACTORS_COUNT+1];   
	T_RULE_ID         m_ruleId;
	CondBase         *m_condRule; //条件指针
	T_ATTRIBUTE_VALUE m_remark;
	
	FeebaseTP & operator = (const FeebaseTP &r) 
	{
		for (int i=0;i < BASETP_FACTORS_COUNT; ++i) 
		{
			m_factors[i] = r.m_factors[i];
		}
		m_tpId          =  r.m_tpId;                              
		m_priority      =  r.m_priority;                            
		m_feeField      =  r.m_feeField;                           
		m_remark        =  r.m_remark;
		m_bizType       =  r.m_bizType;
		m_ruleId        =  r.m_ruleId;
		m_condRule      =  r.m_condRule;
		return *this;
	}
	friend bool operator<(const FeebaseTP &r_left,const FeebaseTP &r_right)
	{
	  if(r_left.m_bizType<r_right.m_bizType) return true;
	  if(r_left.m_bizType>r_right.m_bizType) return false;

	  if(r_left.m_feeField<r_right.m_feeField) return true;
	  if(r_left.m_feeField>r_right.m_feeField) return false;
    return false;
	}
	friend bool operator==(const FeebaseTP &r_left,const FeebaseTP &r_right)
	{
	  return (r_left.m_feeField == r_right.m_feeField &&
	          r_left.m_bizType  == r_right.m_bizType );
  }
  friend ostream& operator<<(ostream& r_os,const FeebaseTP& r_po)
  {
  	r_os<<r_po.m_tpId<<" , "<<
    		r_po.m_priority<<" , "<<
    		r_po.m_feeField<<" , "<<
    		r_po.m_bizType <<" , "<<
    		r_po.m_ruleId ;
        
    for (int i=0;i < BASETP_FACTORS_COUNT; ++i) 
    {
		  r_os<<" , "<<r_po.m_factors[i];
    }
    return r_os;
  }
};



class PO_FeebaseTP
{
  public:
    bool getProperty(PO_Property &property);

    bool convert(const StringVector &argname);

    PO_FeebaseTP(){}

    PO_FeebaseTP(const PO_FeebaseTP& right);

    void operator=(const PO_FeebaseTP& right);

    friend int operator==(const PO_FeebaseTP &left,
    		                  const PO_FeebaseTP &right);

    friend int operator<(const PO_FeebaseTP &left,
    		                 const PO_FeebaseTP& right);

	  friend ostream& operator<<(ostream& os,const PO_FeebaseTP& po);

    FeebaseTP m_record;

  private:
	  static PO_Property m_property;
};
#endif /* PO_CFEEBASETP_H */

