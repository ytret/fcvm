(local lib {})

(fn lib.run [text]
  "Runs the preprocessor on 'text' and returns the processed string."
  (fn split-lines [text]
    "Splits 'text' into a list of lines."
    (icollect [line (string.gmatch text "[^\r\n]+")] line))

  (fn strip-lines [lines]
    "Removes whitespace at the start and end of each line of 'lines'."
    (icollect [_ line (ipairs lines)]
      (string.gsub line "^%s*(.-)%s*$" "%1")))

  (-> (split-lines text)
      (strip-lines)))

lib

