
```
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_JNI_BINDING=ON \
         -DENABLE_HTTP=OFF \
         -DBUILD_EXAMPLES=OFF \
         -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake  \
         -DANDROID_PLATFORM=android-21 \
         -DANDROID_ABI=arm64-v8a
```
