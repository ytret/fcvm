(import-macros {: enum*} :auxm)
(local fennel (require :fennel))

(local lib {:type (enum* [:colon
                          :comma
                          :open-sq
                          :close-sq
                          :arithm-op
                          :comment
                          :id
                          :string
                          :number])})

(fn lib.run [lines]
  "Parses 'lines' into a list of lists of tokens."
  (fn tokenize-line [line]
    "Parses 'line' into a list of tokens."
    (let [tokens {}]
      (var curr-token {})

      (fn push-curr-token []
        (if curr-token.type (table.insert tokens curr-token))
        (set curr-token {}))

      (fn push-token [type val]
        (push-curr-token)
        (set curr-token {: type : val})
        (push-curr-token))

      (fn add-token-char [type ch must-be-sep?]
        (if (= curr-token.type type)
            (set curr-token.val (.. curr-token.val ch))
            (if must-be-sep?
                (error (.. "unexpected character: " ch))
                (do
                  (push-curr-token)
                  (set curr-token {: type :val ch})))))

      (each [ch (string.gmatch line ".")]
        (if (or (= curr-token.type lib.type.comment) (= ch ";"))
            (add-token-char lib.type.comment ch)
            (or (and (= curr-token.type nil) (string.find ch "[%d]"))
                (and (= curr-token.type lib.type.number)
                     (string.find ch "[%u%l%d-_.]")))
            (add-token-char lib.type.number ch)
            (= ch "\"")
            (if (= curr-token.type lib.type.string)
                (push-curr-token)
                (set curr-token {:type lib.type.string :val ""}))
            (= curr-token.type lib.type.string)
            (add-token-char lib.type.string ch)
            (string.find ch "[%u%l%d_.]")
            (add-token-char lib.type.id ch)
            (= ch "[")
            (push-token lib.type.open-sq "[")
            (= ch "]")
            (push-token lib.type.close-sq "]")
            (= ch "+")
            (push-token lib.type.arithm-op "+")
            (= ch "-")
            (push-token lib.type.arithm-op "-")
            (= ch ":")
            (push-token lib.type.colon ":")
            (= ch ",")
            (push-token lib.type.comma ",")
            (= ch " ")
            (push-curr-token)
            (error (.. "unexpected character: " ch))))
      (push-curr-token)
      tokens))

  (icollect [_ line (ipairs lines)]
    (tokenize-line line)))

lib

