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
 */
package com.example.hellojni;

import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;
import java.util.concurrent.TimeUnit;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.LinearLayout;


public class HelloJni extends Activity {

	private static String TAG = "HelloJni";
	private int mNativeContext; // accessed by native methods
	private Handler mEventHandler = new Handler() {
		public void handleMessage(Message msg) {
			Log.e( TAG, "handleMessage: " + msg.what );
		};
	};
	
	@Override
	protected void finalize() throws Throwable {
		super.finalize();
		native_release();
	}
	public HelloJni() {
		WeakReference<HelloJni> weak_thiz = new WeakReference<HelloJni>(this);
		native_setup( weak_thiz );
	}
	
	private Thread hNetworkThread;
	private Socket mSocket;
	private static final int kBufferSize = 188;
	
	private void nonBlockingClient( String remoteIP, int remotePort )
	        throws IOException, InterruptedException {
        SocketChannel channel = SocketChannel.open();
 
        // we open this channel in non blocking mode
        channel.configureBlocking(false);
        channel.connect(new InetSocketAddress(remoteIP, remotePort));
 
        while (!channel.finishConnect()) {
            Log.v( TAG, "still connecting" );
        }
        
        int total = 0;
        int good = 0;
        // see if any message has been received
        ByteBuffer buffer = ByteBuffer.allocate(kBufferSize);
        int offset = 0;
        int readBytes = 0;
        do {
            readBytes = channel.read( buffer );
            Log.v( TAG, "readBytes: " + readBytes );
            if ( readBytes > 0 ) {
                ++total;
                if ( readBytes == kBufferSize ) {               
                    ++good;                                        
                }
                else {
//                    Log.v( TAG, "buffer[0]: " + buffer.get(0) );
//                    Log.v( TAG, "buffer[1]: " + buffer.get(1) );                    
                }
                buffer.flip();
            }            
            else if ( -1 == readBytes ) {
                break;
            }
            
        } while ( true );
        
        Log.v( TAG, "end of socket" );
        Log.v( TAG, "total: " + total );
        Log.v( TAG, "good: " + good );
        channel.close();        
	}
	
	private void blockingClient( String remoteIP, int remotePort ) {
	    Socket socket = new Socket();
        try {
            socket.connect( new InetSocketAddress( remoteIP, remotePort ));
        } catch (IOException e) {
            e.printStackTrace();
        }
        InputStream inStream = null;
        try {
            inStream = socket.getInputStream();
        } catch (IOException e) {
            e.printStackTrace();
        }
        
        int total = 0;
        int good = 0;
        
        byte[] buffer = new byte [kBufferSize];
        int offset = 0;
        int readBytes = 0;
        do {
            try {
                readBytes = inStream.read( buffer, offset, buffer.length-offset );
                Log.v( TAG, String.format( "offset: %d, length: %d, readBytes: %d", 
                    offset, buffer.length-offset, readBytes 
                ));
            } catch (IOException e) {
                e.printStackTrace();
                break;
            }
            if ( -1 == readBytes ) {
                break;
            }
            else if ( 0 == readBytes ) {
                try {
                    TimeUnit.MILLISECONDS.sleep( 1000 );
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                Log.v( TAG, "try again" );
            }
            else if ( readBytes > 0 ) {
                offset += readBytes;
                if ( offset == buffer.length ) {                                    
                    ++total;
                    if ( 0x47 == buffer[0] ) {
                        ++good;
                    }
                    else {
                        Log.v( TAG, "buffer[0]: " + buffer[0] );
                        Log.v( TAG, "buffer[1]: " + buffer[1] );
                    }
                    offset = 0;
                }                                
            }
        } while ( true );
        Log.v( TAG, "end of socket" );
        Log.v( TAG, "total: " + total );
        Log.v( TAG, "good: " + good );
        if ( inStream != null ) {
            try {
                inStream.close();
            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }                            
        }
        if ( socket != null ) {
            try {
                socket.close();
            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }                            
        }
	}
	
	@Override
	protected void onDestroy() {
	    super.onDestroy();
	    Log.d(TAG, "onDestory");
	    if (hNetworkThread != null) { 
	        hNetworkThread.interrupt();
	        hNetworkThread = null;
	    }
	}	 

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);		
		Context context = this;
		hNetworkThread = null;
		
		Button button1 = new Button( context ); 
		button1.setText( "send blocking" );
		button1.setOnClickListener( new OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.d( TAG, "onClick" );
                hNetworkThread = new Thread() {
                    public void run() {
                        native_command( 0 );
                    }
                };
                hNetworkThread.start();
            }
        });
		Button button2 = new Button( context );
        button2.setText( "send non-blocking fixed" );
        button2.setOnClickListener( new OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.d( TAG, "onClick" );
                hNetworkThread = new Thread() {
                    public void run() {
                        native_command( 1 );
                    }
                };
                hNetworkThread.start();
            }
        });
		Button button3 = new Button( context );
        button3.setText( "recv blocking" );
		button3.setOnClickListener( new OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.d( TAG, "recv blocking" );
                hNetworkThread = new Thread() {
                    public void run() {
                        blockingClient( "192.168.0.111", 9999 );
                    }
                };
                hNetworkThread.start();
            }
        });
		Button button4 = new Button( context );
        button4.setText( "recv non-blocking" );
        button4.setOnClickListener( new OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.d( TAG, "recv non-blocking" );
                hNetworkThread = new Thread() {
                    public void run() {
                        try {
                            nonBlockingClient( "192.168.0.111", 9999 );
                        } catch (IOException e) {
                            e.printStackTrace();
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                };
                hNetworkThread.start();
            }
        });
		
		LinearLayout layout = new LinearLayout( context );
		layout.setOrientation( LinearLayout.VERTICAL );
		layout.addView( button1 );
		layout.addView( button2 );
		layout.addView( button3 );
		layout.addView( button4 );
		setContentView( layout );
	}

	private static void notify( Object weak_thiz, int what, 
			int arg1, int arg2 ) {
		HelloJni thiz = (HelloJni) ((WeakReference) weak_thiz).get();
		if (thiz == null) {
			return;
		}

		if (thiz.mEventHandler != null) {
			Message m = thiz.mEventHandler.obtainMessage(what, arg1, arg2);
			thiz.mEventHandler.sendMessage(m);
		}
	}
	private static int getInt( Object weak_thiz ) {
		HelloJni thiz = (HelloJni) ((WeakReference) weak_thiz).get();
		if (thiz == null) {
			return -1;
		}
		int value = 100;
		Log.e( TAG, "Hello::getInt: " + value );
		return value;
	}	
	
	private native void native_testCallBack();
	private native void native_hell();
	private native static void native_init();
	private native void native_setup( Object weak_thiz );
	private native void native_release();
	private native int native_command( int cmd );
	
	static {
		System.loadLibrary("hello-jni");
		native_init();
	}
}
