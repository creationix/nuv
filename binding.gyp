{
    "targets": [{
        "target_name": "nuv",
        "sources": [ "./src/nuv.c" ],
        "include_dirs": [
            "<!(node -e \"require('napi-macros')\")"
        ],
    }],
}
