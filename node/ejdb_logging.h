/**************************************************************************************************
 *  EJDB database library http://ejdb.org
 *  Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
 *
 *  This file is part of EJDB.
 *  EJDB is free software; you can redistribute it and/or modify it under the terms of
 *  the GNU Lesser General Public License as published by the Free Software Foundation; either
 *  version 2.1 of the License or any later version.  EJDB is distributed in the hope
 *  that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *  You should have received a copy of the GNU Lesser General Public License along with EJDB;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA.
 *************************************************************************************************/

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#ifndef EJDB_LOGGING_H
#define	EJDB_LOGGING_H

#include <string>

namespace ejdb {

    class EJError;

    //No throw exception
#define EJ_NO_THROW throw()
#define EJ_THROW(E) throw(E)
#define EJ_THROW_ERR throw(ejdb::EJError)

    //Logging
#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_INFO 3
#define LOG_DEBUG 4


#define EJ_LOG_EJERROR(err) ejdb::ej_log_mherror(__FILE__, __LINE__, err)
#define EJ_ERRNO_THROW(errno_rv, msg) throw ejdb::EJError(msg, errno_rv, __FILE__, __LINE__)
#define EJ_LOG_ERRNO_THROW(errno_rv, msg) ejdb::ej_log_errno_status(__FILE__, __LINE__, errno_rv, msg); throw ejdb::MHError(msg, errno_rv, __FILE__, __LINE__)
#define EJ_LOG_ERRNO_STATUS(errno_rv, msg) ejdb::ej_log_errno_status(__FILE__, __LINE__, errno_rv, msg)

#define EJ_LOG_ERROR(fmt,...) ej_log_intern(LOG_ERROR, __FILE__, __LINE__, fmt,##__VA_ARGS__)
#define EJ_LOG_WARN(fmt,...) ej_log_intern(LOG_WARNING, __FILE__, __LINE__, fmt,##__VA_ARGS__)
#define EJ_LOG_INFO(fmt,...) ej_log_intern(LOG_INFO, __FILE__, __LINE__, fmt,##__VA_ARGS__)

#ifdef _DEBUG
#define EJ_LOG_DBG(fmt, ...) ej_log_intern(LOG_DEBUG, __FILE__, __LINE__, fmt,##__VA_ARGS__)
#else
#define EJ_LOG_DBG(fmt, ...)
#endif

    void ej_log_intern(int priority, const char* file, int line, const char* fmt, ...);
    void ej_log_ejerror(const char* file, int line, const EJError& err);
    void ej_log_errno_status(const char* file, int line, int errno_rv, const std::string& msg);
    void ej_log_errno_status(const char* file, int line, int errno_rv, const char* msg);


#define EJ_ERRNO_MSG(err) ej_errno_msg(err)
    const char* ej_errno_msg(int errno_rv);

    /**
     * Exception class
     */
    class EJError {
    public:

        //exception message
        const std::string msg;
        //error code
        const int errno_code;

        const std::string location;
        const size_t line;

    public:

        virtual void errPrint();

    public:

        EJError(const EJError& src) : msg(src.msg), errno_code(src.errno_code), location(src.location), line(src.line) {
        }

        EJError(const char* _msg, int _errno_code = 0, const char* _location = "", size_t _line = 0) :
        msg(_msg), errno_code(_errno_code), location(_location), line(_line) {
        };

        EJError(const std::string& _msg, int _errno_code = 0, const char* _location = "", size_t _line = 0) :
        msg(_msg), errno_code(_errno_code), location(_location), line(_line) {
        }

        virtual ~EJError() {
        }
    };

}



#endif	/* EJDB_LOGGING_H */

