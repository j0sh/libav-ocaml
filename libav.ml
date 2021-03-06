module Libav = struct

type avframe
type avinput
type pixfmt =
    | YUV420P
    | RGB24
type dim = int * int (* width, height *)
external libav_init_c : unit -> unit = "libav_init"
external open_input_c : string -> avinput = "open_input"
external get_dim_c : avinput -> dim = "get_dim"
external set_swscale_c: avinput -> pixfmt -> dim -> unit =
    "set_swscale"
external get_frame_c : avinput -> avframe = "get_frame"
external get_image_c : string -> avframe = "get_image"
external write_image_c : avframe -> string -> unit = "write_image"
external to_matrix_c : avframe -> 'a array array = "frame2ocaml"
external from_matrix_c : 'a array array -> avframe = "ocaml2frame"

let init () =
    let _ = Callback.register_exception "eof exception" End_of_file in
    libav_init_c ()

let open_input = open_input_c

let get_dim = get_dim_c

let set_swscale = set_swscale_c

let get_frame = get_frame_c

let get_image fname =
    let fmt = open_input fname in
    set_swscale fmt RGB24 (get_dim fmt);
    get_frame fmt

let write_image = write_image_c

let to_matrix = to_matrix_c

let from_matrix = from_matrix_c

end
