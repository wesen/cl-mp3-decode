(in-package :mp3dec)

(defvar *shared-library-directories* `(,(namestring (make-pathname :device :unspecific
                                                                   :name nil
                                                                   :type :unspecific
                                                                   :version :unspecific
                                                                   :defaults mp3dec.system:*mp3dec-directory*))
                                       "/usr/local/lib/"
                                       "/usr/lib/"
                                       "/usr/lib/cl-gd/"
                                       "/cygwin/usr/local/lib/"
                                       "/cygwin/usr/lib/")
  "A list of directories where UFFI tries to find libmp3dec.so")
(defvar *shared-library-types* '("so" "dll")
  "The list of types a shared library can have. Used when looking for
libmp3dec.so")
(defvar *shared-library-drive-letters* '("C" "D" "E" "F" "G")
  "The list of drive letters (used by Wintendo) used when looking for
libmp3dec.so.")
(defvar *mp3dec-supporting-libraries* '("c" "m" "mp3lame")
  "The libraries which are needed by libmp3dec.so. Only needed for
Python-based Lisps like CMUCL, SBCL, or SCL.")

