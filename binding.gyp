{
  "targets": [{
    "target_name": "eyenative",
    "cflags!": [ "-fno-exceptions" ],
    "cflags_cc!": [ "-fno-exceptions" ],
    "sources": [
      "src/FfmpegProcess.cpp",
      "src/FrameThread.cpp",
      "src/main.cpp",
      "src/Native.cpp",
      "src/PipeReader.cpp",
      "src/PreviewThread.cpp",
      "src/Thread.cpp",
      "src/Wrapper.cpp",
    ],
    'include_dirs': [
      "<!@(node -p \"require('node-addon-api').include\")",
    ],
    'dependencies': [
      "<!(node -p \"require('node-addon-api').gyp\")"
    ],
    'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
    'conditions': [
      ['OS=="linux"', {
        'include_dirs': [],
        'library_dirs': []
      }],
      ['OS=="mac"', {
        'include_dirs': [
          "opencv/mac/include/"
        ],
        'library_dirs': [
          "../opencv/mac/lib/"
        ],
        'xcode_settings': {
          "MACOSX_DEPLOYMENT_TARGET": "10.15"
        },
        'libraries': [
          "-lopencv_core",
          "-lopencv_imgproc",
          "-lopencv_highgui",
          "-lopencv_features2d"
        ]
      }],
      ['OS=="win"', {
        'include_dirs': [
          "opencv/win/include/"
        ],
        'library_dirs': [
          "opencv/win/lib/"
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': '1',    
            'AdditionalOptions': ['/EHsc']
          }
        },
        'libraries': [
          "opencv_world451.lib"
        ],
		"copies":[
		  {
		    'destination': './build/Release',
			'files':[
			  'opencv/win/lib/opencv_world451.dll'
			]
	      }
        ]	  
      }]
    ]
  }]
}
