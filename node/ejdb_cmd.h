/*
 * File:   ejdb_cmd.h
 * Author: adam
 *
 * Created on October 30, 2012, 11:41 AM
 */

#ifndef EJDB_CMD_H
#define	EJDB_CMD_H

#include <uv.h>
#include <v8.h>
#include <string>


namespace ejdb {

    template < typename T, typename D = void > class EIOCmdTask {
    public:

        //uv request
        uv_work_t uv_work;

        v8::Persistent<v8::Function> cb;
        T* wrapped;

        //cmd spec
        int cmd;
        D* cmd_data;

        //cmd return data
        int cmd_ret;
        int cmd_ret_data_length;
        std::string cmd_ret_msg;

        //entity type
        int entity;

        //Pointer to free_cmd_data function
        void (*free_cmd_data)(EIOCmdTask<T, D>*);


    public:

        static void free_val(EIOCmdTask<T, D>* dtask) {
            if (dtask->cmd_data) {
                free(dtask->cmd_data);
                dtask->cmd_data = NULL;
            }
        }

        static void delete_val(EIOCmdTask<T, D>* dtask) {
            if (dtask->cmd_data) {
                delete dtask->cmd_data;
                dtask->cmd_data = NULL;
            }
        }

    public:

        EIOCmdTask(const v8::Handle<v8::Function>& _cb, T* _wrapped, int _cmd,
                D* _cmd_data, void (*_free_cmd_data)(EIOCmdTask<T, D>*)) :
        wrapped(_wrapped), cmd(_cmd), cmd_data(_cmd_data), cmd_ret(0), cmd_ret_data_length(0), entity(0) {

            this->free_cmd_data = _free_cmd_data;
            this->cb = v8::Persistent<v8::Function>::New(_cb);
            this->wrapped->Ref();
            this->uv_work.data = this;
        }

        virtual ~EIOCmdTask() {
            this->cb.Dispose();
            this->wrapped->Unref();
            if (this->free_cmd_data) {
                this->free_cmd_data(this);
            }
        }
    };
}








#endif	/* EJDB_CMD_H */

