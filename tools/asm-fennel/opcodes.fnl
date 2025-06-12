(local fennel (require :fennel))
(local prs (require :parsing))

(local lib {})

(local mov
       {:mov_rr {:opcode 32 :opd-cats [prs.cat.reg prs.cat.reg] :size 3}
        :mov_vr {:opcode 33 :opd-cats [prs.cat.reg prs.cat.v32] :size 6}})

(local str
       {:str_rv0 {:opcode 35 :opd-cats [prs.cat.m32 prs.cat.reg] :size 6}
        :str_ri0 {:opcode 34 :opd-cats [prs.cat.ri0 prs.cat.reg] :size 3}
        :str_ri8 {:opcode 36 :opd-cats [prs.cat.ri8 prs.cat.reg] :size 4}
        :str_ri32 {:opcode 37 :opd-cats [prs.cat.ri32 prs.cat.reg] :size 7}
        :str_rir {:opcode 38 :opd-cats [prs.cat.rir prs.cat.reg] :size 4}})

(local ldr
       {:ldr_rv0 {:opcode 39 :opd-cats [prs.cat.reg prs.cat.m32] :size 6}
        :ldr_ri0 {:opcode 40 :opd-cats [prs.cat.reg prs.cat.ri0] :size 3}
        :ldr_ri8 {:opcode 41 :opd-cats [prs.cat.reg prs.cat.ri8] :size 4}
        :ldr_ri32 {:opcode 42 :opd-cats [prs.cat.reg prs.cat.ri32] :size 7}
        :ldr_rir {:opcode 43 :opd-cats [prs.cat.reg prs.cat.rir] :size 4}})

(local add
       {:add_rr {:opcode 66 :opd-cats [prs.cat.reg prs.cat.reg] :size 3}
        :add_rv {:opcode 65 :opd-cats [prs.cat.reg prs.cat.v32] :size 6}})

(local sub
       {:sub_rr {:opcode 68 :opd-cats [prs.cat.reg prs.cat.reg] :size 3}
        :sub_rv {:opcode 67 :opd-cats [prs.cat.reg prs.cat.v32] :size 6}})

(local _not {:not_r {:opcode 87 :opd-cats [prs.cat.reg] :size 2}})

(local _and
       {:and_rr {:opcode 76 :opd-cats [prs.cat.reg prs.cat.reg] :size 3}
        :and_rv {:opcode 75 :opd-cats [prs.cat.reg prs.cat.v32] :size 6}})

(local shr
       {:shr_rr {:opcode 84 :opd-cats [prs.cat.reg prs.cat.reg] :size 3}
        :shr_rv {:opcode 83 :opd-cats [prs.cat.reg prs.cat.v5] :size 3}})

(local cmp {:cmp_rr {:opcode 90 :opd-cats [prs.cat.reg prs.cat.reg] :size 3}})

(local tst
       {:tst_rr {:opcode 92 :opd-cats [prs.cat.reg prs.cat.reg] :size 3}
        :tst_rv {:opcode 85 :opd-cats [prs.cat.reg prs.cat.v32] :size 6}})

(local jmpr {:jmpr_v8 {:opcode 96 :opd-cats [prs.cat.v8] :size 2}})

(local jmpa {:jmpa_v32 {:opcode 97 :opd-cats [prs.cat.v32] :size 5}
             :jmpa_r {:opcode 98 :opd-cats [prs.cat.reg] :size 2}})

(local jeqr {:jeqr_v8 {:opcode 100 :opd-cats [prs.cat.v8] :size 2}})
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
                  : add
                  : sub
                  :and _and
                  :not _not
                  : shr
                  : cmp
                  : tst
                  ;; Flow control
                  : jmpr
                  : jmpa
                  : jeqr
                  : jner
                  : call
                  : ret
                  ;; Other
                  : halt
                  : iret
                  ;;
                  })

lib

