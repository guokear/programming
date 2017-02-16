#include "ControlMemMgr.h"

ControlMemMgr::ControlMemMgr()
{
  m_pCtlInfo=NULL;
}
ControlMemMgr::~ControlMemMgr()
{
  m_pCtlInfo=NULL;
}
void ControlMemMgr::initialize(MdbCtlInfo &r_ctlInfo)
{
  m_pCtlInfo = &r_ctlInfo;
  SpaceInfo  t_spaceInfo;
  
  strcpy(t_spaceInfo.m_spaceName,r_ctlInfo.m_ctlSpaceName);
  strcpy(t_spaceInfo.m_dbName,r_ctlInfo.m_dbName);
  strcpy(t_spaceInfo.m_fileName,r_ctlInfo.m_ctlDiskFile);
  t_spaceInfo.m_size=r_ctlInfo.m_size;
  t_spaceInfo.m_type=CONTROL_SPACE;
  t_spaceInfo.m_spaceCode=1;//控制区编号1
  SpaceMgrBase::initialize(t_spaceInfo);
}
/**
 *createControlSpace 创建控制区表空间.
 *@param   r_flag : 0 第一次创建,1 从文件导入信息
 *@return  true 创建成功,false 失败
 */
bool ControlMemMgr::createControlSpace(const int &r_flag)
{
  if(r_flag==0)//第一次创建
  {
    if(checkSpaceSize()==false)
    {//空闲大小不够
      #ifdef _DEBUG_
        cout<<"空闲大小不够!"<<__FILE__<<__LINE__<<endl;
      #endif
      return false;
    }
  }
  if(createSpace(m_spaceHeader,r_flag)==false)
  {
    #ifdef _DEBUG_
      cout<<"createSpace false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  return true;
}
bool ControlMemMgr::initCtlDataInfo(const int &r_flag)
{
  //////////////////////////////////////////////////////
  //初始化信息,或从文件中导入
  if(r_flag==0) //第一次创建
  {
    return firstCreator();
  }
  else //if(r_flag==1) 
  {
    if(loadDataFromFile()==false)
    {
      #ifdef _DEBUG_
        cout<<"loadDataFromFile() false!"<<__FILE__<<__LINE__<<endl;
      #endif
      return false;
    }
    if(initAfterAttach()==false)
    {
      #ifdef _DEBUG_
        cout<<"loadDataFromFile() false!"<<__FILE__<<__LINE__<<endl;
      #endif
      return false;
    }
    //SESSION信息
    int t_flag=0;
    m_sessionInfoMgr.clear(t_flag);
    return true;
  }
}

/**
 *deleteControlSpace 删除控制区表空间.
 *@return  true 创建成功,false 失败
 */
bool ControlMemMgr::deleteControlSpace()
{
  return deleteSpace(m_spaceHeader);
}

bool ControlMemMgr::firstCreator()
{
  size_t  t_offSet=0;
  //1. 初始化控制区表空间头m_spaceHeader信息
  memcpy(m_pSpHeader,&m_spaceHeader,sizeof(SpaceInfo));t_offSet+=sizeof(SpaceInfo);
  //2. 初始化全局信息
  m_pGobalInfos=(MDbGlobalInfo*)(m_spaceHeader.m_shmAddr+t_offSet);
  m_pGobalInfos->init();
  t_offSet+=sizeof(MDbGlobalInfo);
  //3. 表空间信息初始化
  if(m_spInfoMgr.initElements(m_spaceHeader.m_shmAddr,t_offSet,m_pCtlInfo->m_gparamMaxSpNum)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_spInfoMgr.initElements false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_spInfoMgr.getSize();
  //4. 初始化全局参数信息
  if(m_gParamMgr.initElements(m_spaceHeader.m_shmAddr,t_offSet,m_pCtlInfo->m_gparamMaxNum)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_gParamMgr.initElements false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_gParamMgr.getSize();
  //5.初始化表描述符信息
  if(m_tbDescMgr.initElements(m_spaceHeader.m_shmAddr,t_offSet,m_pCtlInfo->m_tableMaxNum)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_tbDescMgr.initElements false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_tbDescMgr.getSize();
  //6.初始化索引描述符信息
  if(m_idxDescMgr.initElements(m_spaceHeader.m_shmAddr,t_offSet,m_pCtlInfo->m_indexMaxNum)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_idxDescMgr.initElements false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_idxDescMgr.getSize();
  //7.初始化Session信息列表
  if(m_sessionInfoMgr.initElements(m_spaceHeader.m_shmAddr,t_offSet,m_pCtlInfo->m_sessionMaxNum)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_sessionInfoMgr.initElements false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_sessionInfoMgr.getSize();
  //最后设置保留空间信息
  m_pGobalInfos->setReserve(t_offSet);
  return true;
}


//校验大小是否足够
bool ControlMemMgr::checkSpaceSize()
{
  size_t  t_needSize=0;
  //1. 计算所需的空间大小
  t_needSize+=sizeof(SpaceInfo);//表空间头
  t_needSize+=sizeof(MDbGlobalInfo);//全局信息 
  //表空间信息
  t_needSize+=sizeof(NodeListInfo)+m_pCtlInfo->m_gparamMaxSpNum*sizeof(NodeTmpt<SpaceInfo>);
  //全局参数信息
  t_needSize+=sizeof(NodeListInfo)+m_pCtlInfo->m_gparamMaxNum*sizeof(NodeTmpt<GlobalParam>);
  //表描述符信息
  t_needSize+=sizeof(NodeListInfo)+m_pCtlInfo->m_tableMaxNum*sizeof(NodeTmpt<TableDesc>);
  //索引描述符信息
  t_needSize+=sizeof(NodeListInfo)+m_pCtlInfo->m_indexMaxNum*sizeof(NodeTmpt<IndexDesc>);
  //SESSION信息
  t_needSize+=sizeof(NodeListInfo)+m_pCtlInfo->m_sessionMaxNum*sizeof(NodeTmpt<SessionInfo>);
  
  //2. 比较
  if(t_needSize>m_pCtlInfo->m_size)
  {
    #ifdef _DEBUG_
      cout<<"所需空闲最小为："<<t_needSize
          <<" 目前设置的空间为:"<<m_pCtlInfo->m_size
          <<" 空间大小不够!"<<endl;
    #endif
    return false;
  }
  #ifdef _DEBUG_
    cout<<"所需空闲最小为："<<t_needSize
        <<" 目前设置的空间为:"<<m_pCtlInfo->m_size
        <<" !"<<endl;
  #endif  
  return true;
}

unsigned int ControlMemMgr::getSpaceCode()
{//该操作携带写操作
  unsigned int t_spaceCode;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      t_spaceCode=-1;return t_spaceCode;
    }
  }catch(Mdb_Exception &e)
  {
    t_spaceCode=-1;return t_spaceCode;
  }
  t_spaceCode = m_pGobalInfos->getNextSpCode();
  m_pdbLock->releaseWriteLock();
  return t_spaceCode;
}

bool ControlMemMgr::addSpaceInfo(const SpaceInfo &r_spaceInfo)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      t_bRet = false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet = false;return t_bRet;
  }  
  if(m_spInfoMgr.addElement(r_spaceInfo)==false)
  {
    t_bRet = false;
  }
  m_pdbLock->releaseWriteLock();  
  updateSpTime();
  return t_bRet;
}
bool ControlMemMgr::getSpaceInfo(const char * r_spaceName,SpaceInfo &r_spaceInfo)
{
  bool t_bRet=true;
  SpaceInfo t_spaceInfo;
  strcpy(t_spaceInfo.m_spaceName,r_spaceName);
  try
  {
    if(m_pdbLock->getReadLock()==false)
    {
      t_bRet = false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet = false;return t_bRet;
  }  
  t_bRet = m_spInfoMgr.getElement(t_spaceInfo,r_spaceInfo);
  m_pdbLock->releaseReadLock();  
  return t_bRet;
}
bool ControlMemMgr::getSpaceInfos(vector<SpaceInfo> &r_spaceInfo)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getReadLock()==false)
    {
      t_bRet = false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet = false;return t_bRet;
  }  
  if(m_spInfoMgr.getElements(r_spaceInfo)==false)
  {
    t_bRet = false;
  }
  m_pdbLock->releaseReadLock();  
  return t_bRet;
}
//获取SessionId
int  ControlMemMgr::getSessionId()
{
  if(m_pGobalInfos==NULL) return -1;
  int t_iret=-1;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return t_iret;
    }
  }catch(Mdb_Exception &e)
  {
    return t_iret;
  }  
  t_iret = m_pGobalInfos->getSessionId();
  m_pdbLock->releaseWriteLock();  
  return t_iret;
}
//释放SessionId
bool ControlMemMgr::realseSid(const int &r_sid)
{
  if(m_pGobalInfos==NULL) return false;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    return false;
  }  
  m_pGobalInfos->releadSid(r_sid);
  m_pdbLock->releaseWriteLock();  
  return true;
}
bool ControlMemMgr::registerSession(SessionInfo &r_sessionInfo)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      cout<<"m_pdbLock->getWriteLock() false!"<<__FILE__<<__LINE__<<endl;
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    cout<<"false here ! "<<__FILE__<<__LINE__<<endl;
    return false;
  }  
  if(m_sessionInfoMgr.addElement(r_sessionInfo)==false)
  {
    cout<<"false here ! "<<__FILE__<<__LINE__<<endl;
    t_bRet = false;
  }
  m_pdbLock->releaseWriteLock();  
  return t_bRet;
}
bool ControlMemMgr::unRegisterSession(const SessionInfo &r_sessionInfo)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    return false;
  }  
  if(m_sessionInfoMgr.deleteElement(r_sessionInfo)==false)
  {
    t_bRet = false;
  }
  m_pdbLock->releaseWriteLock();  
  return t_bRet;
}
bool ControlMemMgr::getSessionInfos(vector<SessionInfo> &r_sessionInfoList)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getReadLock()==false)
    {
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    return false;
  }  
  if(m_sessionInfoMgr.getElements(r_sessionInfoList)==false)
  {
    t_bRet = false;
  }
  m_pdbLock->releaseReadLock();  
  return t_bRet;
}
//重新初始化Session列表 add by gaojf 2009-3-2 3:54
bool ControlMemMgr::reInitSessionInfos()
{
  m_sessionInfoMgr.reInit();
  return true;
} 
///end

///<attach方式初始化
bool ControlMemMgr::attach_init()
{
  //设置ShmKey
  T_FILENAMEDEF  t_ctlFileName;
  sprintf(t_ctlFileName,"%s/ctl/%s.ctl",getenv(MDB_HOMEPATH.c_str()),m_spaceHeader.m_dbName);
  m_spaceHeader.m_shmKey = ftok(t_ctlFileName,m_spaceHeader.m_spaceCode);
  if(m_spaceHeader.m_shmKey<0) 
  {
    #ifdef _DEBUG_
    cout<<"ftok("<<t_ctlFileName<<",";
    cout<<m_spaceHeader.m_spaceCode<<") false!"<<endl;
    #endif
    return false;
  }
  if(SpaceMgrBase::attach()==false)
  {
    #ifdef _DEBUG_
      cout<<"SpaceMgrBase::attach() false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  return initAfterAttach();
}
bool ControlMemMgr::initAfterAttach()
{
  size_t  t_offSet=0;
  //1. 跳过控制区表空间头m_spaceHeader信息
  t_offSet+=sizeof(SpaceInfo); 
  //2. attach初始化全局信息
  m_pGobalInfos=(MDbGlobalInfo*)(m_spaceHeader.m_shmAddr+t_offSet);t_offSet+=sizeof(MDbGlobalInfo);
  //3. 表空间信息初始化
  if(m_spInfoMgr.attach_init(m_spaceHeader.m_shmAddr,t_offSet)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_spInfoMgr.attach_init false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_spInfoMgr.getSize();
  //4. 初始化全局参数信息
  if(m_gParamMgr.attach_init(m_spaceHeader.m_shmAddr,t_offSet)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_gParamMgr.attach_init false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_gParamMgr.getSize();
  //5.初始化表描述符信息
  if(m_tbDescMgr.attach_init(m_spaceHeader.m_shmAddr,t_offSet)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_tbDescMgr.attach_init false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_tbDescMgr.getSize();
  //6.初始化索引描述符信息
  if(m_idxDescMgr.attach_init(m_spaceHeader.m_shmAddr,t_offSet)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_idxDescMgr.attach_init false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_idxDescMgr.getSize();
  //7.初始化Session信息列表
  if(m_sessionInfoMgr.attach_init(m_spaceHeader.m_shmAddr,t_offSet)==false)
  {
    #ifdef _DEBUG_
      cout<<"m_sessionInfoMgr.attach_init false!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  t_offSet+=m_sessionInfoMgr.getSize();
  return true;
}

bool ControlMemMgr::addTableDesc(const TableDesc &r_tableDesc,TableDesc* &r_pTableDesc)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      t_bRet=false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet=false;return t_bRet;
  }
  int t_flag=1;//支持表描述符重用
  if(m_tbDescMgr.addElement(r_tableDesc,r_pTableDesc,t_flag)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseWriteLock();  
  updateTbDescTime();
  return t_bRet;
}

//删除表描述符时：前提，表必须已经truncate，且对应的索引也全部删除
bool ControlMemMgr::deleteTableDesc(const TableDesc &r_tableDesc)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      t_bRet=false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet=false;return t_bRet;
  }
  int  t_flag=1;
  if(m_tbDescMgr.deleteElement(r_tableDesc,t_flag)==false)
  {
    t_bRet = false;
  }
  m_pdbLock->releaseWriteLock();  
  updateTbDescTime();
  return t_bRet;
}
bool ControlMemMgr::getTableDescList(vector<TableDesc> &r_tableDescList)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getReadLock()==false)
    {
      t_bRet=false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet=false;return t_bRet;
  }
  if(m_tbDescMgr.getElements(r_tableDescList)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseReadLock();  
  return t_bRet;
}
bool ControlMemMgr::getTableDescByName(const char *r_tableName,TableDesc* &r_pTableDesc)
{
  TableDesc t_tableDesc;
  strcpy(t_tableDesc.m_tableDef.m_tableName,r_tableName);
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getReadLock()==false)
    {
      t_bRet=false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet=false;return t_bRet;
  }
  if(m_tbDescMgr.getElement(t_tableDesc,r_pTableDesc)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseReadLock();  
  return t_bRet;
}
bool ControlMemMgr::addIndexDesc(const IndexDesc &r_indexDesc,IndexDesc* &r_pIndexDesc)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      t_bRet=false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet=false;return t_bRet;
  }
  int t_flag=1;//支持表描述符重用
  if(m_idxDescMgr.addElement(r_indexDesc,r_pIndexDesc,t_flag)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseWriteLock();  
  updateIndexTime();
  updateTbDescTime();
  return t_bRet;
}

bool ControlMemMgr::deleteIndexDesc(const IndexDesc &r_indexDesc)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      t_bRet=false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet=false;return t_bRet;
  }
  int  t_flag=1;
  if(m_idxDescMgr.deleteElement(r_indexDesc,t_flag)==false)
  {
    t_bRet = false;
  }
  m_pdbLock->releaseWriteLock();  
  updateIndexTime();
  updateTbDescTime();
  return t_bRet;
}
bool ControlMemMgr::getIndexDescByName(const char *r_indexName,IndexDesc* &r_pIndexDesc)
{
  IndexDesc t_indexDesc;
  strcpy(t_indexDesc.m_indexDef.m_indexName,r_indexName);
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getReadLock()==false)
    {
      t_bRet=false;return t_bRet;
    }
  }catch(Mdb_Exception &e)
  {
    t_bRet=false;return t_bRet;
  }
  if(m_idxDescMgr.getElement(t_indexDesc,r_pIndexDesc)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseReadLock();
  return t_bRet;
  
}
void ControlMemMgr::getIndexDescByPos(const size_t &r_offSet,IndexDesc* &r_pIndexDesc)
{
  r_pIndexDesc = (IndexDesc*)(getSpaceAddr()+r_offSet);
}
void ControlMemMgr::updateTbDescTime()
{
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return;
    }
  }catch(Mdb_Exception &e)
  {
    return;
  }
  m_pGobalInfos->updateTbTime();
  m_pdbLock->releaseWriteLock();
}
void ControlMemMgr::updateIndexTime()
{
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return;
    }
  }catch(Mdb_Exception &e)
  {
    return;
  }
  m_pGobalInfos->updateIdxTime();
  m_pdbLock->releaseWriteLock();
}
void ControlMemMgr::updateSpTime()
{
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return;
    }
  }catch(Mdb_Exception &e)
  {
    return;
  }
  m_pGobalInfos->updateSpTime();
  m_pdbLock->releaseWriteLock();
}
void ControlMemMgr::updateDbTime()
{
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return;
    }
  }catch(Mdb_Exception &e)
  {
    return;
  }
  m_pGobalInfos->updateDbTime();
  m_pdbLock->releaseWriteLock();
}

bool ControlMemMgr::addGlobalParam(const GlobalParam &r_gParam)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    return false;
  }
  int t_flag=0;
  if(m_gParamMgr.addElement(r_gParam,t_flag)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseWriteLock();
  return t_bRet;
}
bool ControlMemMgr::getGlobalParam(const char *r_paramname,GlobalParam &r_gParam )
{
  if(strlen(r_paramname)>=sizeof(T_NAMEDEF))
  {
    #ifdef _DEBUG_
      cout<<"名称过长!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  GlobalParam t_gParam;
  strcpy(t_gParam.m_paramName,r_paramname);
  
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getReadLock()==false)
    {
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    return false;
  }
  if(m_gParamMgr.getElement(t_gParam,r_gParam)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseReadLock();
  return false;
}
bool ControlMemMgr::getGlobalParams(vector<GlobalParam>& r_gparamList)
{
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getReadLock()==false)
    {
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    return false;
  }
  if(m_gParamMgr.getElements(r_gparamList)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseReadLock();
  return t_bRet;
}

bool ControlMemMgr::updateGlobalParam(const char *r_paramname,const char *r_value)
{
  GlobalParam t_gParam;
  if(strlen(r_paramname)>=sizeof(T_NAMEDEF)||
     strlen(r_value)>=sizeof(T_GPARAMVALUE))
  {
    #ifdef _DEBUG_
      cout<<"名称或值过长!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  strcpy(t_gParam.m_paramName,r_paramname);
  strcpy(t_gParam.m_value,r_value);
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    return false;
  }
  if(m_gParamMgr.updateElement(t_gParam)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseWriteLock();
  return t_bRet;
}
bool ControlMemMgr::deleteGlobalParam(const char *r_paramname)
{
  if(strlen(r_paramname)>=sizeof(T_NAMEDEF))
  {
    #ifdef _DEBUG_
      cout<<"名称过长!"<<__FILE__<<__LINE__<<endl;
    #endif
    return false;
  }
  int t_flag=0;
  GlobalParam t_gParam;
  strcpy(t_gParam.m_paramName,r_paramname);
  bool t_bRet=true;
  try
  {
    if(m_pdbLock->getWriteLock()==false)
    {
      return false;
    }
  }catch(Mdb_Exception &e)
  {
    return false;
  }
  if(m_gParamMgr.deleteElement(t_gParam,t_flag)==false)
  {
    t_bRet=false;
  }
  m_pdbLock->releaseWriteLock();
  return t_bRet;
}

//调试接口
bool ControlMemMgr::dumpSpaceInfo(ostream &r_os)
{
  r_os<<"---------表空间:"<<m_pSpHeader->m_spaceName<<"内容 起始!---------"<<endl;
  SpaceMgrBase::dumpSpaceInfo(r_os);
  r_os<<"---------数据库全局信息----------------------"<<endl;
  r_os<<"m_spUpTime:"<<ctime(&(m_pGobalInfos->m_spUpTime))<<endl;
  r_os<<"m_tbUpTime:"<<ctime(&(m_pGobalInfos->m_tbUpTime))<<endl;
  r_os<<"m_idxUpTime:"<<ctime(&(m_pGobalInfos->m_idxUpTime))<<endl;
  r_os<<"m_nextSpCode:"<<m_pGobalInfos->m_nextSpCode<<endl;
  r_os<<"m_curSessionId:"<<m_pGobalInfos->m_curSessionId<<endl;
  r_os<<"------SESSION状态列表(已占用)----------------"<<endl;
  for(int i=0;i<MAX_SESSION_NUM;i++)
  {
    if(m_pGobalInfos->m_sidState[i])
    {
      r_os<<"  "<<i<<" "<<m_pGobalInfos->m_sidState[i]<<endl;
    }
  }
  r_os<<"-------------------------------------"<<endl;
  r_os<<"m_reserve:"<<m_pGobalInfos->m_reserve<<endl;
  r_os<<"--------------------------------------------"<<endl;
  r_os<<"---------表空间信息----------------------"<<endl;
  m_spInfoMgr.dumpInfo(r_os);
  r_os<<"-----------------------------------------"<<endl;
  r_os<<"---------全局参数信息----------------------"<<endl;
  m_gParamMgr.dumpInfo(r_os);
  r_os<<"-----------------------------------------"<<endl;
  r_os<<"---------表描述符信息----------------------"<<endl;
  m_tbDescMgr.dumpInfo(r_os);
  r_os<<"-----------------------------------------"<<endl;
  r_os<<"---------索引描述符信息----------------------"<<endl;
  m_idxDescMgr.dumpInfo(r_os);
  r_os<<"-----------------------------------------"<<endl;
  r_os<<"---------SESSION信息----------------------"<<endl;
  m_sessionInfoMgr.dumpInfo(r_os);
  r_os<<"-----------------------------------------"<<endl;
  r_os<<"---------表空间:"<<m_pSpHeader->m_spaceName<<"内容 终止!---------"<<endl;
  return true;
}
