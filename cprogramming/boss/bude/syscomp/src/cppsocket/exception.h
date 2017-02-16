/***************************************************************************
 *   CPPSocket - library                                                   *
 *                                                                         *
 *   Copyright (C) 2001 by Ralf-Christian Juergensen                       *
 *                         <rcj@users.sourceforge.net>                     *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This library is distributed in the hope that it will be usefull, but  *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      *
 *   Library General Public License for more details.                      *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston,                *
 *   MA 02111-1307, USA                                                    *
 ***************************************************************************/

#ifndef CPPSOCKET_EXCEPTION_H
#define CPPSOCKET_EXCEPTION_H

#include <errno.h>

#ifdef _AIX
#define _USE_IRS
#endif
#include <netdb.h>
#undef _USE_IRS

#if defined(hpux) || defined(__hpux__) || defined(__hpux)
#undef hstrerror
static const char *hstrerror( int err )
{
        switch (err) {
        case HOST_NOT_FOUND:    return "Host not found (authoritative)";
        case TRY_AGAIN:         return ("Host not found (server fail?)");
        case NO_RECOVERY:       return ("Non-recoverable failure");
        case NO_DATA:           return ("No data of requested type");
#ifdef NETDB_INTERNAL
        case NETDB_INTERNAL:    return STRERROR( errno );
#endif
        }
        return ("Unknown resolver error");
}
#endif

#include <string>
#include <exception>

namespace CPPSocket
{
  using std::string;
  using std::exception;

  /** Baseclass for all exceptions in cppsocket-library
    *
    * @author Ralf-Christian J黵gensen
    */
  class Exception : public exception
  {
  public:
    Exception(const string& _method, const string& _message, int _code) throw()
      : method(_method), message(_message), code(_code)
    {}
    
    virtual ~Exception() throw() {}
    
    const string& getMethod()  const throw() { return method;  }
    const string& getMessage() const throw() { return message; }
    int getCode()              const throw() { return code;    }

    virtual const char* what() const throw() { return getMessage().c_str(); }
    
  protected:
    string method;  ///< Method, which throwed the eception
    string message; ///< Errormessage
    int code;       ///< Errorcode
  };

  /** Special class for @c errno errors.
    *
    * @author Ralf-Christian J黵gensen
    */
  class LibCException : public Exception
  {
  public:
    /// Use @c errno as errorcode and errormessage.
    LibCException(const string& method) throw()
      : Exception(method, strerror(errno), errno)
    {}
  };

  /** Special class for @c h_errno errors.
    *
    * @author Ralf-Christian J黵gensen
    */
  class HLibCException : public Exception
  {
  public:
    /// Use @c h_errno as errorcode and errormessage
    HLibCException(const string& method) throw()
      : Exception(method, hstrerror(h_errno), h_errno)
    {}
  };
}
#endif
