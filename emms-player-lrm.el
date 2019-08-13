;;; emms-player-lrm.el --- Lelo Remote Music for EMMS -*- lexical-binding: t -*-

;; Copyright (C) 2019 by Jakub Wojciech

;; This file is part of Lelo Remote Music Player.

;; Lelo Remote Music Player is free software: you can redistribute it
;; and/or modify it under the terms of the GNU General Public License
;; as published by the Free Software Foundation, either version 3 of
;; the License, or (at your option) any later version.

;; Lelo Remote Music Player is distributed in the hope that it will be
;; useful, but WITHOUT ANY WARRANTY; without even the implied warranty
;; of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
;; General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with Lelo Remote Music Player. If not, see
;; <https://www.gnu.org/licenses/>.

(require 'emms-player-simple)

(defgroup emms-player-lrm nil
  "EMMS player for Lelo Remote Music Player."
  :group 'emms-player
  :prefix "emms-player-lrm-")

(defcustom emms-player-lrm (emms-player 'emms-player-lrm-start
					'emms-player-lrm-stop
					'emms-player-lrm-playable-p)
  "*Parameters for the Lelo Remote Music Player.*"
  :type '(cons symbol alist)
  :group 'emms-player-lrm)


(defcustom emms-player-lrm-executable "lrm-control"
  "An executable for the Lelo Remote Music Player."
  :type '(string)
  :group 'emms-player-lrm)

(defcustom emms-player-lrm-config nil
  "A config file for the Lelo Remote Music Player.

It's unnecessary if `emms-player-lrm-port', `emms-player-lrm-host',
`emms-player-lrm-passphrase' are all set.

These variables take precedence over `emms-player-lrm-config'."
  :type '(file :must-match t)
  :group 'emms-player-lrm)

(defcustom emms-player-lrm-host nil
  "The host where Lelo Remote Music Server resides."
  :type '(string)
  :group 'emms-player-lrm)

(defcustom emms-player-lrm-port nil
  "The port for Lelo Remote Music Server."
  :type '(integer)
  :group 'emms-player-lrm)

(defcustom emms-player-lrm-passphrase nil
  "The passphrase used for making requests to Lelo Remote Music Server."
  :type '(string)
  :group 'emms-player-lrm)

(defcustom emms-player-lrm-cert nil
  "The SSL certificate file for Lelo Remote Music Server."
  :type '(file :must-match t)
  :group 'emms-player-lrm)

(emms-player-set emms-player-lrm
		 'regex
                 (apply #'emms-player-simple-regexp
			emms-player-base-format-list))

(emms-player-set emms-player-lrm
		 'pause
		 'emms-player-lrm-toggle-pause)

(emms-player-set emms-player-lrm
		 'resume
		 'emms-player-lrm-toggle-pause)

;; (emms-player-set emms-player-lrm
;; 		 'seek
;; 		 'emms-player-lrm-seek)

;; (emms-player-set emms-player-lrm
;; 		 'seek-to
;; 		 'emms-player-lrm-seek-to)

;; NOTE: Returning an error on failure is probably temporary.
;;       The user probably shouldn't be bothered by the backend.

(defun emms-player-lrm--ensure-daemon ()
  (with-temp-buffer
    (call-process emms-player-lrm-executable nil t nil "ping")
    (goto-char (point-min))
    (when (looking-at-p "Daemon not running.")
      (let (args)
	(dolist (pair (seq-filter #'cadr
				  `(("--host" ,emms-player-lrm-host)
				    ("--port" ,emms-player-lrm-port)
				    ("--pass" ,emms-player-lrm-passphrase)
				    ("--config" ,emms-player-lrm-config)))
		      args)
	  (setq args (append pair args)))
	(eval `(emms-player-lrm-command-1 "daemon" ,@args))))))

;; -------------------- Synchronous --------------------
(defun emms-player-lrm-command-1 (&rest args)
  (with-temp-buffer
    (let* ((default-directory
	     (file-name-directory emms-player-lrm-executable))
	   (result (eval (append '(call-process emms-player-lrm-executable nil
						(current-buffer) nil)
				 args))))
      (when (/= 0 result)
	(goto-char (point-min))
	(error (buffer-substring (point) (point-at-eol))))
      result)))

(defun emms-player-lrm-command (&rest args)
  (emms-player-lrm--ensure-daemon)
  (apply #'emms-player-lrm-command-1 args))

;; -------------------- Asynchronous --------------------
(defun emms-player-lrm-command--sentinel (process event)
  (cond
   ((string-match "exited abnormally with code" event)
    (with-current-buffer (process-buffer process)
      (goto-char (point-min))
      (let ((err-message (buffer-substring (point) (point-at-eol))))
	(kill-buffer (current-buffer))
	(error err-message))))
   ((string-match "finished" event)
    (kill-buffer (process-buffer process)))))

(defun emms-player-lrm-command-async (&rest args)
  ;; wait for the process buffer to be deleted which will mean that the
  ;; previous process finished
  (when-let* ((buffer (get-buffer "*emms-player-lrm*")))
    (cl-do* ((timeout 2000)
    	     (sleep 500)
    	     (time 0 (+ time sleep)))
    	((or (not (buffer-live-p buffer)) (>= time timeout)))
      (sleep-for 0 sleep)))
  (make-thread
   (lambda ()
     (emms-player-lrm--ensure-daemon)
     (let ((default-directory
     	     (file-name-directory emms-player-lrm-executable))
     	   (buffer (get-buffer-create "*emms-player-lrm*")))
       (make-process
     	:name "Lelo Remote Music"
     	:buffer buffer
     	:command (cons emms-player-lrm-executable args)
     	:sentinel #'emms-player-lrm-command--sentinel))))
  nil)
;; ------------------------------------------------------

(defun emms-player-lrm-start (track)
  "Starts a process playing TRACK."
  (emms-player-lrm-command-async "play" (emms-track-name track))
  ;; TODO: Check if play was successful, only then set a player as started
  (emms-player-started 'emms-player-lrm))

(defun emms-player-lrm-stop ()
  "Stop the currently playing track."
  (when (= 0 (emms-player-lrm-command "stop"))
    (emms-player-stopped)))

(defun emms-player-lrm-playable-p (track)
  "Check if a TRACK can be played."
  (and (memq (emms-track-type track) '(file url streamlist playlist))
       (string-match (emms-player-get emms-player-lrm 'regex)
		     (emms-track-name track))
       (= 0 (emms-player-lrm-command "ping"))))

(defun emms-player-lrm-toggle-pause ()
  "Pause the currently playing track."
  (emms-player-lrm-command "toggle-pause"))

(defun emms-player-lrm-seek (amount)
  "Seek by AMOUNT seconts. Can be a positive or a negative number."
  (error "Not implemented")
  (emms-player-lrm-command "seek" (concat
				   (when (>= amount 0) "+")
				   (number-to-string amount))))

(defun emms-player-lrm-seek-to (pos)
  "Seek to POS seconds in the currently playing track."
  (error "Not implemented")
  (emms-player-lrm-command "seek" (max 0 pos)))

(defun emms-volume-lrm-change (amount)
  "Change volume by AMOUNT. Can be positive or negative."
  (interactive "MVolume change amount: ")
  (unless (or (eq (elt amount 0) ?-)
	      (eq (elt amount 0) ?+))
    (setq amount (concat "+" amount)))
  (emms-player-lrm-command "volume" amount))

;; TODO: Remove these debugging variables
(setq emms-player-lrm-executable
      "~/Programming/remote-music-control/builddir/remote-control"
      emms-player-lrm-config
      "/home/lampilelo/Programming/remote-music-control/builddir/lrm.conf"
      emms-player-lrm-host
      "localhost"
      emms-player-lrm-cert
      "/home/lampilelo/Programming/remote-music-control/builddir/server.crt")
(add-to-list 'emms-player-list 'emms-player-lrm)

(provide 'emms-player-lrm)

;; (emms-player-lrm-start (emms-track 'file "/data/lampilelo/Music/AC-DC - The Razors Edge (1990)/01-Thunderstruck.mp3"))
;; (emms-player-lrm-stop)
;; (emms-player-lrm-command "stop")

;;; emms-player-lrm.el ends here
