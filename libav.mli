module Libav : sig

    type avframe
    type avinput
    type pixfmt =
    | YUV420P
    | RGB24
    val init: unit -> unit
    val open_input: string -> avinput
    val set_swscale: avinput -> pixfmt -> int -> int -> unit
    val get_frame: avinput -> avframe
    val get_image : string -> avframe
    val write_image : avframe -> string -> unit
    val to_matrix : avframe -> 'a array array
    val from_matrix : 'a array array -> avframe

end
