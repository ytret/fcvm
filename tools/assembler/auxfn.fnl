(local lib {})

(fn lib.is-in-list? [list val]
  "Returns 'true' if 'list' contains 'val', otherwise returns 'false'."
  (var found false)
  (each [_ v (ipairs list) &until found]
    (set found (= v val)))
  found)

lib

