
################################################################################
#环境相关的宏定义
#  该Makefile配置必须依赖一组关联的配置文件，这些配置文件默认存放在 
#  ${BUDE_HOME}/stdmake 下。
################################################################################
# 设置系统要依赖的环境变量
include $(BUDE_HOME)/stdmake/stdenv

# 设置当前组建或应用应属的工作目录
WORK_HOME = ${BUDE_HOME}/syscomp

################################################################################
#编译源和生成物定义
################################################################################

# 需要关联编译当前目录下子目录组建或应用。子目录名,用空格分隔多个子目录 
SUBDIRS      =

# 所有需要要编译的源文件,用空格分隔,用\换行
SRCS         = SoAdapterFactory.cpp main.cpp OcciFlowLineDao.cpp  

#要提交到$(DESTINC)的头文件,用空格分隔,用\换行
#定义要提交的头文件。一般只有对其他组建或应用开放的头文件进行提交
SUBMIT_HDRS  = 

#指定要提交到$(DESTSRC)的makefile文件名称
#makefile,Makefile默认提交，其他文件名需要在下面指定
#SUBMIT_MAKEFILE =

#生成的目标程序
PROGRAM        = fl
#生成的目标静态库
STATIC_LIBRARY = 
#生成的目标动态库
SHARE_LIBRARY  = 


#提交时, 可执行文件所要拷贝的目录
DEST    = $(WORK_HOME)/bin
#提交时, 库文件所要拷贝的目录
DESTLIB = $(WORK_HOME)/lib
#提交时, 头文件所要拷贝的目录
DESTINC = $(WORK_HOME)/include/flowline
#提交时, 源代码所要拷贝的目录
DESTSRC =
#提交时, 文档所要拷贝的目录
DESTDOC = 

################################################################################
#该组建或应用所要依赖的头和库文件的目录
################################################################################
THE_INCLUDE      = -D_LARGE_FILE -I../engine/src -I$(WORK_HOME)/include \
		   -I${BUDE_HOME}/lang/include -I${BUDE_HOME}/lang/include/base
THE_LIBPATH      =   -L$(WORK_HOME)/lib  -L$(BUDE_HOME)/lang/lib -L../engine
THE_LIB          = -llog4cpp -ldataset -lfl -lnsl -lbase -locci -lclntsh


################################################################################
##编译相关的参数定义
################################################################################
#内部已经包含平台的头目录,这里指定编译需要额外的参数
#编译时,可以选择的参数:
#动态库文件     $(SHARE_CXX_FLAGS)
#ORACLE数据库   $(ORA_INCLUDE)
#SYBASE数据库   $(SYB_INCLUDE)
#举例:
#CXXFLAGS= $(SHARE_CXX_FLAGS) $(ORA8I_INCLUDE)
#CCFLAGS
CXXFLAGS= -DOFF_DEBUG -DDEBUG_SQL -D_REENTRANT  $(SHARE_CXX_FLAGS) ${THE_INCLUDE} ${ORA_INCLUDE} ${SYSCOMP_INCLUDE}

#内部已经包含平台的库目录,这里指定连接时需要额外的参数
#编译时,可以选择的参数:
#ORACLE数据库   $(ORA_LIBPATH)
#SYBASE数据库   $(SYB_LIBPATH)
#举例:
#LDFLAGS= $(ORA_LIBPATH) 
LDFLAGS = ${THE_LIBPATH} ${ORA_LIBPATH} ${SYSCOMP_LIBPATH}

#指定该组件或应用所依赖的库名
#编译时,可以选择的参数:
#ORACLE数据库   $(ORA_LIB)
#SYBASE数据库   $(SYB_LIB)
#举例:
#LDLIB  = $(ORA8I_LIB) $(SYB_LIB) -lm
LDLIB   = ${THE_LIB} ${ORA_LIB}

###############################################################################
##编译的makefile定义，不要修改
################################################################################
#包含预定义的 makefile
include $(BUDE_HOME)/stdmake/stdmk

