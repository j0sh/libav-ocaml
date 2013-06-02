module Libav : sig

    type avframe
    val init: unit -> unit
    val get_image : string -> avframe
    val write_image : avframe -> string -> unit
    val to_matrix : avframe -> 'a array array
    val from_matrix : 'a array array -> avframe

end
