(import-macros {: match-exact*} :auxm)
(local {: is-in-list?} (require :auxfn))
(local fennel (require :fennel))
(local opcodes (require :opcodes))
(local prs (require :parsing))

(local lib {})

(fn lib.resolve-names [instrs]
  "Resolves each non-pseudo-instruction in 'instrs' to a single opcode.

  Uses opcode descriptors from 'opcodes.descs'. Sets the key 'gen-desc' of
  'instr' to the matched opcode descriptor.
  "
  (fn resolve-name [instr]
    (local num-instr-opds
           (if (= nil instr.operands) 0 (length instr.operands)))

    (fn opd-matches? [desc-opd-cat instr-opd]
      "Returns true if the operand 'instr-opd' matches with the operand category
      'desc-opd-cat'."
      ;; HACK: at this stage labels are still not resolved and are stored as
      ;; operands with category [id]; here we treat them as [id v32].
      (let [instr-opd-cats (match-exact* instr-opd.cats ;;
                                         [prs.cat.id] [prs.cat.id prs.cat.v32]
                                         ;;
                                         _ instr-opd.cats)]
        (is-in-list? instr-opd-cats desc-opd-cat)))

    (fn opcode-matches? [desc instr]
      (let [num-desc-opds (length desc.opd-cats)
            same-num-opds (= num-desc-opds num-instr-opds)
            opds-match (faccumulate [opds-match true i 1 num-desc-opds
                                     &until (not opds-match)]
                         (and opds-match
                              (opd-matches? (. desc.opd-cats i)
                                            (. instr.operands i))))]
        (and same-num-opds opds-match)))

    (assert (not= nil instr.name))
    (when (= nil (?. opcodes :descs instr.name))
      (error (.. "unrecognized instruction name '" instr.name "' in:\n"
                 (fennel.view instr))))
    (var resolved? false)
    (each [_ desc (pairs (. opcodes.descs instr.name)) &until resolved?]
      (when (opcode-matches? desc instr)
        (tset instr :gen-desc desc)
        (set resolved? true)))
    (when (not resolved?)
      (error (.. "unrecognized instruction:\n" (fennel.view instr))))
    instr)

  (icollect [_ instr (ipairs instrs)]
    (if (or (= nil instr.name) (= 1 (string.find instr.name "%.")))
        instr
        (resolve-name instr))))
