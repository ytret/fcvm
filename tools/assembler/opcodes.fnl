(local fennel (require :fennel))
(local prs (require :parsing))

(local lib {})

(local mov
       {:mov_rr {:opcode 32 :opd-cats [prs.cat.reg prs.cat.reg] :size 2}
        :mov_vr {:opcode 33 :opd-cats [prs.cat.reg prs.cat.v32] :size 6}})

(local str
       {:str_rv0 {:opcode 35 :opd-cats [prs.cat.m32 prs.cat.reg] :size 6}
        :str_ri0 {:opcode 34 :opd-cats [prs.cat.ri0 prs.cat.reg] :size 2}
        :str_ri8 {:opcode 36 :opd-cats [prs.cat.ri8 prs.cat.reg] :size 3}
        :str_ri32 {:opcode 37 :opd-cats [prs.cat.ri32 prs.cat.reg] :size 6}
        :str_rir {:opcode 38 :opd-cats [prs.cat.rir prs.cat.reg] :size 3}})

(local ldr
       {:ldr_rv0 {:opcode 39 :opd-cats [prs.cat.reg prs.cat.m32] :size 6}
        :ldr_ri0 {:opcode 40 :opd-cats [prs.cat.reg prs.cat.ri0] :size 2}
        :ldr_ri8 {:opcode 41 :opd-cats [prs.cat.reg prs.cat.ri8] :size 3}
        :ldr_ri32 {:opcode 42 :opd-cats [prs.cat.reg prs.cat.ri32] :size 6}
        :ldr_rir {:opcode 43 :opd-cats [prs.cat.reg prs.cat.rir] :size 3}})

(local _not {:not_r {:opcode 87 :opd-cats [prs.cat.reg] :size 2}})

(local tst
       {:tst_rr {:opcode 92 :opd-cats [prs.cat.reg prs.cat.reg] :size 2}
        :tst_rv {:opcode 85 :opd-cats [prs.cat.reg prs.cat.v32] :size 6}})

(local jmpr {:jmpr_v8 {:opcode 96 :opd-cats [prs.cat.v8] :size 2}})

(local jmpa {:jmpa_v32 {:opcode 97 :opd-cats [prs.cat.v32] :size 5}
             :jmpa_r {:opcode 98 :opd-cats [prs.cat.reg] :size 2}})

(local jner {:jner_v8 {:opcode 104 :opd-cats [prs.cat.v8] :size 2}})

(local call
       {:calla_v32 {:opcode 125 :opd-cats [prs.cat.v32] :size 5}
        :calla_r {:opcode 126 :opd-cats [prs.cat.reg] :size 2}})

(local ret {:ret {:opcode 127 :opd-cats [] :size 1}})

(local halt {:halt {:opcode 161 :opd-cats [] :size 1}})
(local iret {:iret {:opcode 163 :opd-cats [] :size 1}})

(tset lib :descs {;; Data movement
                  : mov
                  : str
                  : ldr
                  ;; ALU
                  :not _not
                  : tst
                  ;; Flow control
                  : jmpr
                  : jmpa
                  : jner
                  : call
                  : ret
                  ;; Other
                  : halt
                  : iret
                  ;;
                  })

lib

