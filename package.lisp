(in-package :cl-user)

(defpackage :mp3dec
  (:use :cl :uffi)
  (:export :mp3dec-play-file))
