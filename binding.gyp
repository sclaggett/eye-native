{
  "targets": [{
    "target_name": "<(module_name)",
    "cflags!": [ "-fno-exceptions" ],
    "cflags_cc!": [ "-fno-exceptions" ],
    "sources": [
      "src/FrameThread.cpp",
      "src/FfmpegProcess.cpp",
      "src/main.cpp",
      "src/NativeWrapper.cpp",
      "src/PipeReader.cpp",
      "src/Thread.cpp",
    ],
    'product_dir': '<(module_path)',
    'include_dirs': [
      "<!@(node -p \"require('node-addon-api').include\")",
    ],
    'libraries': [
      "-lopencv_core",
      "-lopencv_imgproc",
      "-lopencv_highgui",
      "-lopencv_features2d",
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
        }
      }],
      ['OS=="win"', {
        'include_dirs': [],
        'library_dirs': []
      }],
    ],
  }]
}
