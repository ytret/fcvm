(local tok (require :scanning))
(local fennel (require :fennel))

(local lib {})

(fn lib.run [token-lines]
  "Parses each of 'token-lines' into a list of instructions."
  (λ filter-comments [tokens]
    (icollect [_ token (ipairs tokens)]
      (when (not= tok.type.comment (. token :type)) token)))
  (λ parse-line [tokens]
    "Parses 'tokens' into an instruction."
    (let [has-label? (and (>= (length tokens) 2)
                          (= tok.type.id (. tokens 1 :type))
                          (= tok.type.colon (. tokens 2 :type)))
          opcode-idx (if has-label? 3 1)
          has-opcode? (>= (length tokens) opcode-idx)
          label (when has-label? (. tokens 1 :val))
          opcode-type (?. tokens opcode-idx :type)
          opcode (when has-opcode?
                   (if (= tok.type.id opcode-type)
                       (. tokens opcode-idx :val)
                       (error (.. "expected token type id, got " opcode-type ""
                                  " on token-line:\n" (fennel.view tokens)))))
          first-opd-idx (+ 1 opcode-idx)
          operands {}]
      (for [i first-opd-idx (length tokens)]
        ;; By an 'operand' I mean here any token after the opcode.
        (let [opd-num (- i first-opd-idx)
              opd-tok (. tokens i)
              opd-type (. opd-tok :type)
              need-comma? (= 1 (% opd-num 2)) ; every second token = comma
              ]
          (if need-comma?
              (when (not= tok.type.comma opd-type)
                (error (.. "expected a comma token at index " i ":\n"
                           (fennel.view tokens))))
              (if (or (= tok.type.id opd-type) (= tok.type.string opd-type)
                      (= tok.type.number opd-type))
                  (table.insert operands opd-tok)
                  (error (.. "expected token type id, string or number, got "
                             opd-type " on token-line:\n" (fennel.view tokens)))))))
      (values (or has-label? has-opcode?) {: label : opcode : operands})))
  (icollect [_ tokens (ipairs token-lines)]
    (let [(not-empty? instr) (parse-line (filter-comments tokens))]
      (when not-empty? instr))))

lib

