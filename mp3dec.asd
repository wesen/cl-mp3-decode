(in-package :cl-user)

(defpackage :mp3dec.system
  (:use :cl :asdf)
  (:export :*mp3dec-directory*))

(in-package :mp3dec.system)

(defparameter *mp3dec-directory*
  (make-pathname :name nil :type nil :version nil
		 :defaults (parse-namestring *load-truename*)))

(defsystem :mp3dec
    :components ((:file "package")
		 (:file "specials" :depends-on ("package"))
		 (:file "init" :depends-on ("specials"))
		 (:file "mp3dec-uffi" :depends-on ("init")))
    :depends-on (:uffi))
