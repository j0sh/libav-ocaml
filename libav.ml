module Libav = struct

type avframe
external libav_init_c : unit -> unit = "libav_init"
external get_image_c : string -> avframe = "get_image"
external write_image_c : avframe -> string -> unit = "write_image"
external to_matrix_c : avframe -> 'a array array = "frame2ocaml"
external from_matrix_c : 'a array array -> avframe = "ocaml2frame"

let init () =
    let _ = Callback.register_exception "eof exception" End_of_file in
    libav_init_c ()

let get_image = get_image_c

let write_image = write_image_c

let to_matrix = to_matrix_c

let from_matrix = from_matrix_c

end
