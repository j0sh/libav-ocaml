type avframe
external to_matrix : avframe -> 'a array array = "frame2ocaml"
external from_matrix : 'a array array -> avframe = "ocaml2frame"
external get_image : string -> avframe = "get_image"
external write_image : avframe -> string -> unit = "write_image"
