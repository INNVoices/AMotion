{
  "targets": [
    {
      "target_name": "amotion",
      "sources": [
        "src/addon.cpp",
        "../../src/amotion.cpp"
      ],
      "include_dirs": [
        "../../include",
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "defines": [
        "NAPI_CPP_EXCEPTIONS"
      ],
      "cflags": [
        "-std=c++20"
      ],
      "cflags!": [
        "-fno-exceptions"
      ],
      "cflags_cc": [
        "-std=c++20"
      ],
      "cflags_cc!": [
        "-fno-exceptions"
      ],
      "xcode_settings": {
        "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
      },
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "AdditionalOptions": [
            "/std:c++20"
          ]
        }
      }
    }
  ]
}
