/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
 
#define NDK_BUILD

#ifdef NDK_BUILD
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#else
#include "JNIHelp.h"
#endif
 
#include <string.h>
#include <jni.h>
#include <assert.h>
#include <string>

#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/tcp.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <android/log.h>
#define TAG "HELLO-JNI-TAG"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#ifdef NDK_BUILD
static int jniThrowException(JNIEnv* env, const char* className, const char* msg) {  
    jclass exceptionClass = env->FindClass(className);  
    if (exceptionClass == NULL) {  
        __android_log_print(ANDROID_LOG_ERROR,  
                TAG,  
                "Unable to find exception class %s",  
                        className);  
        return -1;  
    }  
  
    if (env->ThrowNew(exceptionClass, msg) != JNI_OK) {  
        __android_log_print(ANDROID_LOG_ERROR,  
                TAG,  
                "Failed throwing '%s' '%s'",  
                className, msg);  
    }  
    return 0;  
}  
#endif

static const char* const kClassPathName = "com/example/hellojni/HelloJni";
struct fields_t {
    JNIEnv *env;
    jclass clazz;
    jobject object;    
    jmethodID post_event;
};
static fields_t fields = { 0 };

static void jni_init( JNIEnv *env )
{       
    jclass clazz = env->FindClass(kClassPathName);
    if (clazz == NULL) {
        return ;
    }    
    fields.post_event = env->GetStaticMethodID(clazz, "notify", "(Ljava/lang/Object;III)V");
    if (fields.post_event == NULL) {
        return ;
    }
}
static void jni_setup( JNIEnv *env, jobject thiz, jobject weak_thiz ) {
    fields.env = env;
    if ( fields.env == NULL ) {
        jniThrowException(env, "java/lang/Exception", NULL);
        return ;
    }
    jclass clazz = env->GetObjectClass(thiz);
    if (clazz == NULL) {
        jniThrowException(env, "java/lang/Exception", NULL);
        return ;
    }  
    fields.clazz = (jclass)env->NewGlobalRef(clazz);
    fields.object = env->NewGlobalRef(weak_thiz);   
}
static void jni_release( JNIEnv *env, jobject thiz ) {
    env->DeleteGlobalRef( fields.object );
    env->DeleteGlobalRef( fields.clazz );
    fields.env = NULL;
}

static void jni_hello( JNIEnv* env, jobject thiz ) {
    LOGE("jni_hello");
    std::string str( "hello" );
    LOGE("test std::string %s", str.c_str() );
}
static void jni_notify( int msg, int ext1, int ext2 ) {
    JNIEnv *env = fields.env;
    env->CallStaticVoidMethod(fields.clazz, fields.post_event, fields.object, msg, ext1, ext2 );
}
static int jni_getInt() {
    // JNIEnv *env = AndroidRuntime::getJNIEnv();    
    JNIEnv *env = fields.env;
    jmethodID methodID = env->GetStaticMethodID(fields.clazz, "getInt", "(Ljava/lang/Object;)I");
    if (methodID == NULL) {
        return -1;
    }
    return env->CallStaticIntMethod( fields.clazz, methodID, fields.object );
}
static void jni_testCallBack(JNIEnv *env, jobject thiz) {
    LOGE( "jni_notify" );
    jni_notify( 0, 0, 0 );    
    LOGE( "jni_getInt: %d", jni_getInt() );
}

int makeSocketNonBlocking(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }
    int res = fcntl(s, F_SETFL, flags | O_NONBLOCK);
    if (res < 0) {
        return -errno;
    }
    return 0;
}

int createBlockingTCPServer( int remotePort ) {
    LOGD( "createBlockingTCPServer" );
    int sockfd;
    int ret;
	struct sockaddr_in dest;
    const int kBufferSize = 188;
	char buffer[kBufferSize];

	/* create socket , same as client */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( sockfd < 0 ) {
        LOGE( "socket failed: %d", errno );
        return -1;
    }
    
    // setup socket
    const int yes = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (ret) {
        LOGE( "setsockopt SO_REUSEADDR failed: %d", errno );
        return -1;
    }
    int flag = 1;
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    if (ret) {
        LOGE( "setsockopt failed: %d", errno );
        return -1;
    }
    int tos = 224;  // VOICE
    ret = setsockopt(sockfd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
    if (ret < 0) {
        LOGE( "setsockopt failed: %d", errno );
        return -1;
    }    

	/* initialize structure dest */
	bzero(&dest, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(remotePort);
	/* this line is different from client */
	dest.sin_addr.s_addr = INADDR_ANY;

	/* Assign a port number to socket */
    LOGV( "bind" );
	ret = bind(sockfd, (struct sockaddr*)&dest, sizeof(dest));
    if ( ret < 0 ) {
        LOGE( "bind failed: %d", errno );
        return -1;
    }

	/* make it listen to socket with max 20 connections */
	ret = listen(sockfd, 20);
    if ( ret < 0 ) {
        LOGE( "bind failed: %d", errno );
        return -1;
    }

	/* infinity loop -- accepting connection from client forever */
	while(1)
	{
		int clientfd;
		struct sockaddr_in client_addr;
		int addrlen = sizeof(client_addr);

		/* Wait and Accept connection */
        LOGV( "accept" );
		clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
        if ( clientfd < 0 ) {
            LOGE( "accept error: %d", errno );
            break;
        }
        // ret = makeSocketNonBlocking(clientfd);
        // if (ret < 0) {
            // LOGE( "makeSocketNonBlocking clientfd failed: %d", errno );
            // break;
        // }

		/* Send message */
        int offset = 0;
        int n = 0;
        for ( int i = 0; i < 10000; ++i ) {
            bzero( buffer, kBufferSize );
            buffer[0] = 0x47;
            buffer[1] = i;
            n = send(clientfd, buffer, kBufferSize, 0);
            LOGV( "buffer[1]: %d, sendsize: %d, sendbytes: %d", 
                buffer[1], kBufferSize, n 
            );
            if ( -1 == n && ECONNRESET == errno ) {
                LOGE( "connection reset" );
                goto bail2;
            }
            LOGV( "send %d done", i );
        }
bail2:
		/* close(client) */
        LOGV( "close client %d", clientfd );
		close(clientfd);
	}
    
	/* close(server) , but never get here because of the loop */
    LOGV( "close socket %d", sockfd );
	close(sockfd);
	return 0;
}

int createTCPClient( const char *remoteIP, int remotePort ) {
    int sockfd;
	struct sockaddr_in dest;
	char buffer[128];

	/* create socket */
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	
	/* initialize value in dest */
	bzero(&dest, sizeof(dest));
	dest.sin_family = PF_INET;
	dest.sin_port = htons(remotePort);
	inet_aton(remoteIP, &dest.sin_addr);

	/* Connecting to server */
    LOGV( "connect %s", remoteIP );
	connect(sockfd, (struct sockaddr*)&dest, sizeof(dest));
	
	/* Receive message from the server and print to screen */
	bzero(buffer, 128);
	recv(sockfd, buffer, sizeof(buffer), 0);
	LOGV( "recv %d %d", buffer[0], buffer[1] );

	/* Close connection */
    LOGV( "close socket %d", sockfd );
	close(sockfd);

	return 0;
}

int createNonBlockingTCPServer( int remotePort ) {
    LOGD( "createNonBlockingTCPServer" );
    int sockfd;
    int ret;
	struct sockaddr_in dest;
    const int kBufferSize = 188;
	char buffer[kBufferSize];

	/* create socket , same as client */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( sockfd < 0 ) {
        LOGE( "socket failed: %d", errno );
        return -1;
    }
    
    // setup socket
    const int yes = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (ret) {
        LOGE( "setsockopt SO_REUSEADDR failed: %d", errno );
        return -1;
    }
    int flag = 1;
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    if (ret) {
        LOGE( "setsockopt TCP_NODELAY failed: %d", errno );
        return -1;
    }
    int tos = 224;  // VOICE
    ret = setsockopt(sockfd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
    if (ret < 0) {
        LOGE( "setsockopt IP_TOS failed: %d", errno );
        return -1;
    }

	/* initialize structure dest */
	bzero(&dest, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(remotePort);
	/* this line is different from client */
	dest.sin_addr.s_addr = INADDR_ANY;

	/* Assign a port number to socket */
    LOGV( "bind" );
	ret = bind(sockfd, (struct sockaddr*)&dest, sizeof(dest));
    if ( ret < 0 ) {
        LOGE( "bind failed: %d", errno );
        return -1;
    }

	/* make it listen to socket with max 20 connections */
	ret = listen(sockfd, 20);
    if ( ret < 0 ) {
        LOGE( "bind failed: %d", errno );
        return -1;
    }

	/* infinity loop -- accepting connection from client forever */
	while(1)
	{
		int clientfd;
		struct sockaddr_in client_addr;
		int addrlen = sizeof(client_addr);

		/* Wait and Accept connection */
        LOGV( "accept" );
		clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
        if ( clientfd < 0 ) {
            LOGE( "accept error: %d", errno );
            break;
        }
        ret = makeSocketNonBlocking(clientfd);
        if (ret < 0) {
            LOGE( "makeSocketNonBlocking clientfd failed: %d", errno );
            break;
        }

		/* Send message */        
        for ( int i = 0; i < 10000; ++i ) {
            bzero( buffer, kBufferSize );
            buffer[0] = 0x47;
            buffer[1] = i;
            int offset = 0;
            int n = 0;
            do {
                n = send(clientfd, &buffer[offset], kBufferSize-offset, 0);   
                LOGV( "buffer[1]: %d, sendsize: %d, sendbytes: %d", 
                    buffer[1], kBufferSize-offset, n 
                );                
                if ( -1 == n ) {
                    LOGE( "errno: %d", errno );
                    if ( ECONNRESET == errno ) {
                        goto bail2;
                    }
                    else if ( EAGAIN == errno ) {
                        usleep( 200000 );
                    }
                    else {
                        LOGE( "unknown error" );
                        goto bail2;
                    }
                }
                else if ( n > 0 ) {
                    offset += n;
                    if ( kBufferSize == offset ) {
                        offset = 0;
                        break;
                    }
                }
                else {
                    LOGE( "unknown status" );
                    goto bail2;
                }
            } while ( true );
            LOGV( "send %d done", i );
        }
bail2:
		/* close(client) */
        LOGV( "close client %d", clientfd );
		close(clientfd);
	}
    
	/* close(server) , but never get here because of the loop */
    LOGV( "close socket %d", sockfd );
	close(sockfd);
	return 0;
}


static void jni_command( JNIEnv *env, jobject thiz, jint cmd ) {
    LOGV( "jni_command: %d", cmd );
    switch ( cmd ) {
    case 0:
        createBlockingTCPServer( 9999 );
        break;
    case 1:
        createNonBlockingTCPServer( 9999 );
        break;
    }
}

static JNINativeMethod gMethods[] = {
    { "native_hell", "()V", (void *)jni_hello, }, 
    { "native_init", "()V", (void *)jni_init, }, 
    { "native_setup", "(Ljava/lang/Object;)V", (void *)jni_setup, }, 
    { "native_release", "()V", (void *)jni_release, }, 
    { "native_testCallBack", "()V", (void *)jni_testCallBack, }, 
    { "native_command", "(I)I", (void*)jni_command, }, 
};

#ifdef NDK_BUILD 
static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) {
        LOGE("Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        LOGE("RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}
static int register_com_example_hellojni_HelloJni(JNIEnv *env)
{
    return registerNativeMethods(env,
        kClassPathName, gMethods, NELEM(gMethods));
}
#else
static int register_com_example_hellojni_HelloJni(JNIEnv *env)
{
    return AndroidRuntime::registerNativeMethods(env,
                kClassPathName, gMethods, NELEM(gMethods));
}
#endif 

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    ALOGV( "v1.0.0.141115" );
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        goto bail;
    }
    assert(env != NULL);

    if (register_com_example_hellojni_HelloJni(env) < 0) {
        ALOGE("ERROR: HelloJni native registration failed\n");
        goto bail;
    }
    /* success -- return valid version number */
    result = JNI_VERSION_1_4;

bail:
    return result;
}
