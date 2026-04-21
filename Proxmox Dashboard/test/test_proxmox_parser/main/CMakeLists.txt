idf_component_register(
    SRCS "test_proxmox_parser.c" "../../../main/proxmox_client.c"
    INCLUDE_DIRS "." "../../../main"
    REQUIRES json unity
)
