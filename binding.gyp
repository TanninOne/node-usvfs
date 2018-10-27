{
    "targets": [
        {
            "target_name": "usvfs",
            "includes": [
                "auto.gypi"
            ],
            "conditions": [
                ['OS=="win"', {
                    "sources": [
                        "src/usvfswrapper.cpp"
                    ],
                }]
            ],
            "include_dirs": [
              "./usvfs/include",
            ],
            "library_dirs": [
              "./usvfs_build/install/libs",
            ],
            "libraries": [
              "usvfs_x64"
            ],
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "defines": [
                "UNICODE",
                "_UNICODE"
            ],
            "msvs_settings": {
                "VCCLCompilerTool": {
                    "ExceptionHandling": 1
                }
            }
        }
    ],
    "includes": [
        "auto-top.gypi"
    ]
}
