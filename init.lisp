(in-package :mp3dec)

(defun load-mp3dec-lib ()
  (let ((filename (find-foreign-library "libmp3dec"
					*shared-library-directories*
					:types *shared-library-types*
					:drive-letters *shared-library-drive-letters*)))
    (load-foreign-library filename :module "mp3dec"
			  :supporting-libraries *mp3dec-supporting-libraries*)
    (print filename)))

(load-mp3dec-lib)
