type avframe
external to_matrix : avframe -> 'a array array = "frame2ocaml"
external get_image : string -> avframe = "get_image"
