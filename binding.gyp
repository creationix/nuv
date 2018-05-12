{
    "targets": [{
        "target_name": "nuv",
        "sources": [ "./nuv.c" ],
        "include_dirs": [
            "<!(node -e \"require('napi-macros')\")"
        ],
    }],
}
