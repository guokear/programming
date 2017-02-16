#ifndef _TABLEMEMMGR_H_INCLUDE_20080425_
#define _TABLEMEMMGR_H_INCLUDE_20080425_
#include "TableSpaceMgr.h"
#include <pthread.h>

class TableMemMgr
{
  public:
    TableMemMgr();
    virtual ~TableMemMgr();
  public:
    //根据表空间定义信息,创建表空间
    bool createTbSpace(SpaceInfo &r_spaceInfo,const int &r_flag);
    bool attachAllTbSpace();
    bool detachAllTbSpace();
    bool deleteAllTbSpace();
    bool attach_int(SpaceInfo &r_spaceInfo);
    //初始化表空间
    bool initSpaceInfo(const int &r_flag);
    bool dumpDataIntoFile();
    //增加一个表空间的管理
    bool addSpaceMgr(TableSpaceMgr* t_tbSpaceMgr)
    {
      m_tableSpaceMgrList.push_back(t_tbSpaceMgr);
      return true;
    }
    void getSpaceInfoList(vector<SpaceInfo> &r_spaceInofList)
    {
      r_spaceInofList.clear();
      for(vector<TableSpaceMgr*>::iterator r_itr = m_tableSpaceMgrList.begin();
         r_itr!=m_tableSpaceMgrList.end();r_itr++)
      {
        r_spaceInofList.push_back(*((*r_itr)->getSpaceInfo()));
      }
    }
  public:
    //申请页面
    bool allocatePage(const char* r_spaceName,PageInfo* &r_pPage,ShmPosition &r_pagePos);
    //释放页面
    bool freePage(const ShmPosition &r_pagePos);
    //根据Slot地址，取对应的页面信息和页面地址
    bool getPageBySlot(const ShmPosition &r_slot,ShmPosition &r_pagePos,PageInfo* &r_pPage);
  protected:
    typedef struct
    {
        pthread_t     m_threadId;	    ///< 线程ID.
        bool          m_bRet;		      ///< 返回值.
        TableSpaceMgr *m_pSpaceMgr;   ///< 表空间管理对象指针
    }ThreadParam;
    static void * dumpDataInfoFile_r(ThreadParam &r_param);
  private:
    vector<TableSpaceMgr*> m_tableSpaceMgrList;
    vector<TableSpaceMgr*>::iterator m_tbMgrItr;
  public:
    //调试接口
    virtual bool dumpSpaceInfo(ostream &r_os);
    // 内存使用率 add by chenm
    // 默认返回所有表空间使用率
    void getTableSpaceUsedPercent(map<string,float> &vUserdPercent,const char * cTableSpaceName);
};

#endif //_TABLEMEMMGR_H_INCLUDE_20080425_
