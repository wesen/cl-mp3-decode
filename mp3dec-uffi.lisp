(in-package :mp3dec)

(defconstant +buf-size+ 4096)
(defconstant +error-string-size+ 256)

(def-struct mp3-state
  (channels :int)
  (little-endian :int)
  (pcm-l (:array :short #.(* 100 +buf-size+)))
  (pcm-r (:array :short #.(* 100 +buf-size+)))
  (pcmlen :unsigned-int)
  (strerror (:array :char #.+error-string-size+))
  (snd-fd :int)
  (mp3-fd :int)
  (initialized :int)
  (lame-initialized :int))

(def-foreign-type mp3-state-ptr (* mp3-state))

(def-function ("mp3dec_error" mp3dec-error)
    ((state mp3-state-ptr))
  :returning :cstring
  :module "mp3dec")

(def-function ("mp3dec_reset" mp3dec-reset)
    ((state mp3-state-ptr))
  :returning :void
  :module "mp3dec")

(def-function ("mp3dec_close" mp3dec-close)
    ((state mp3-state-ptr))
  :returning :void
  :module "mp3dec")

(def-function ("mp3dec_decode_data" mp3dec-decode-data)
    ((state mp3-state-ptr)
     (buf (* :unsigned-char))
     (len :unsigned-int))
  :returning :int
  :module "mp3dec")

(def-array-pointer bap :unsigned-char)

(defmacro with-safe-alloc ((var alloc free) &rest body)
  `(let (,var)
    (unwind-protect
	 (progn (setf ,var ,alloc)
		,@body)
      (when ,var ,free))))

(defun play-file (pathname)
  (with-foreign-object (state 'mp3-state)
    (mp3dec-reset state)
    (unwind-protect
	 (with-open-file (s pathname :direction :input
			    :element-type '(unsigned-byte 8))
	   (let ((arr (make-array 4096 :initial-element 0
				  :element-type '(unsigned-byte 8))))
	     (with-safe-alloc (buf (allocate-foreign-object
				    :unsigned-char +buf-size+)
				   (free-foreign-object buf))
	       (do ((read (read-sequence arr s)
			  (read-sequence arr s)))
		   ((or (null read)
			(= 0 read)) t)
		 (dotimes (i read)
		   (setf (deref-array buf 'bap i) (aref arr i)))
		 (let ((ret (mp3dec-decode-data state buf read)))
		   #+cmu
		   (mp:process-yield)
		   (when (= ret -1)
		     (error (mp3dec-error state))))))))
      (progn
	(warn "Closing mp3 state")
	(mp3dec-close state)))))
