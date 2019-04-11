
```
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_HTTP=OFF -DBUILD_EXAMPLES=OFF -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake  -DANDROID_PLATFORM=android-21 -DANDROID_ABI=arm64-v8a
```

Problems:

```
extern_iowow/src/log/iwlog.c:323:15: warning: incompatible integer to pointer conversion assigning to 'char *' from 'int'
      [-Wint-conversion]
    errno_msg = strerror_r(errno_code, ebuf, EBUF_SZ);
              ^ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
extern_iowow/src/log/iwlog.c:401:13: warning: implicit declaration of function 'basename' is invalid in C99
      [-Wimplicit-function-declaration]
    fname = basename(fnameptr);
            ^
extern_iowow/src/log/iwlog.c:401:11: warning: incompatible integer to pointer conversion assigning to 'char *' from 'int'
      [-Wint-conversion]
    fname = basename(fnameptr);


extern_iowow/src/utils/mt19937ar.c:62:8: fatal error: unknown type name 'pthread_spinlock_t'; did you mean 'pthread_rwlock_t'?
static pthread_spinlock_t lock;
       ^~~~~~~~~~~~~~~~~~
       pthread_rwlock_t


qsort_s
https://github.com/noporpoise/sort_r
```