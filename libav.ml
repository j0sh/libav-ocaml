module Libav = struct

type avframe
external get_image_c : string -> avframe = "get_image"
external write_image_c : avframe -> string -> unit = "write_image"
external to_matrix_c : avframe -> 'a array array = "frame2ocaml"
external from_matrix_c : 'a array array -> avframe = "ocaml2frame"

let get_image = get_image_c

let write_image = write_image_c

let to_matrix = to_matrix_c

let from_matrix = from_matrix_c

end
