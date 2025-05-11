(local fennel (require :fennel))
(local lib {})

(fn lib.enum* [vals]
  (assert-compile (sequence? vals) "expected list for vals" vals)
  (assert-compile (not= 0 (length vals)) "vals must not be empty" vals)
  (let [res {}]
    (each [i v (ipairs vals)]
      (assert-compile (= :string (type v))
                      (.. "vals[" i "] must be a string, not " (type v)) vals)
      (tset res v v))
    res))

(fn lib.match-exact* [val ...]
  "'match' for sequential lists for matching the exact number of elements.

  For example, consider a three-element sequence matched against a two-element
  sequence pattern:

  - 'match' returns 'true' because the first two elements are equal:
    (let [vals [:a :b :c]]
      (print (match vals [:a :b] true _ false)))
    -> true

  - 'match-exact*' returns 'false' because the pattern contains two elements,
    while 'vals' contains three of them:
    (let [vals [:a :b :c]]
      (print (match-exact* [:a :b] true _ false)))
    -> false

  - If 'vals' becomes [:a :b] OR the pattern becomes [:a :b :c], the expression
    will return 'true'.

  Limitations:
  - Subject ('vals') must be a sequential list.
  - Each pattern must be a sequential list without guard clauses or '_' (match
    any value).
  "
  (assert-compile (not= val nil) "missing subject" val)
  (assert-compile (= 0 (% (select "#" ...) 2))
                  "expected even number of pattern/body pairs" ...)
  (assert-compile (not= 0 (select "#" ...))
                  "expected at least one pattern/body pair")
  (local args (pack ...))
  (var orig-pairs [])
  (for [i 1 (length args) 2]
    (table.insert orig-pairs [(. args i) (. args (+ i 1))]))
  (var mod-pairs [])
  (each [i [pattern body] (ipairs orig-pairs)]
    (local pattern-is-_? (and (= :table (type pattern)) (= 1 (length pattern))
                              (= "_" (. pattern 1))))
    (assert-compile (or (sequence? pattern) pattern-is-_?)
                    (.. "pattern #" i " must be a sequence")
                    (select (- (* 2 i) 1) ...))
    (let [seq-length (length pattern)
          mod-pattern (if pattern-is-_? pattern
                          `(where ,pattern (and (= ,seq-length (length ,val)))))]
      (table.insert mod-pairs mod-pattern)
      (table.insert mod-pairs body)))
  `(match ,val ,(unpack mod-pairs)))

lib

