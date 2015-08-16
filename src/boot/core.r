REBOL [
	Title: "REBOL Core extension"
	Purpose: {
		This extension contains commands not yet migrated to the core source code.
	}
	Author: "Richard Smolak"
	Copyright: {2010 2011 2012 2013 Saphirion AG, Zug, Switzerland}
	License: {
		Licensed under the Apache License, Version 2.0.
		See: http://www.apache.org/licenses/LICENSE-2.0
	}
	Name: core
	Type: extension
	Exports: [] ; added by make-host-ext.r
]

words: [
	n			;modulus
	e			;public exponent
	d			;private exponent
	p			;prime num 1
	q			;prime num 2
	dp			;CRT exponent 1
	dq			;CRT exponent 2
	qinv		;CRT coefficient
	priv-key	;private key
	pub-key		;public key
	g			;generator
	pkcs1		;padding type
]

init-words: command [
	words [block!]
]

init-words words

; The Ren/C library itself is not involved in encapping; this is the
; responsibility of the executable built with it to provide.
;
;get-encap-data: command [
;	"Returns encapped binary data"
;]

to-png: command [
	"Save an image to PNG format"
	image [image!]
]

; There is no GUI in Ren/C to pop up directory selection dialogs
;
;<no-export> req-dir: command [
;	"low-level command used by REQUEST-DIR"
;	/title
;		text [string!]
;	/path
;		dir [string!]
;]
;
;request-dir: funct [
;	"Asks user to select a directory and returns it as file path"
;	/title
;		"Custom dialog title text"
;		text [string!]
;	/path
;		"Default directory path"
;		dir [file!]
;][
;	if dir [dir: lib/replace/all to-local-file dir "/" "//"]
;	if result: apply :req-dir [title text path dir] [
;		return to-rebol-file result
;	]
;]

rc4: command [
	"Encrypt/decrypt data(modifies) using RC4 algorithm. Returns stream cipher context handle."
	/key
		crypt-key [binary!] "Crypt key. Have to be provided only for the first time to get stream handle!."
	/stream
		ctx [handle!] "Stream cipher context."
		data [binary! none!] "Data to encrypt/decrypt. Or NONE to close the cipher stream."
]

rsa-make-key: func [
	"Creates a key object for RSA algorithm."
][
	make object! [
		n:			;modulus
		e:			;public exponent
		d:			;private exponent
		p:			;prime num 1
		q:			;prime num 2
		dp:			;CRT exponent 1
		dq:			;CRT exponent 2
		qinv:		;CRT coefficient
		none
	]
]

rsa: command [
	"Encrypt/decrypt data using the RSA algorithm (with default PKCS#1 padding)."
	data [binary!]
	key-object [object!]
	/decrypt "Decrypts the data (default is to encrypt)"
	/private "Uses an RSA private key (default is a public key)"
	/padding "Selects the type of padding to use"
		padding-type [word! none!] "Type of padding. Available values: PKCS1 or NONE"
]


dh-make-key: func [
	"Creates a key object for Diffie-Hellman algorithm."
;NOT YET IMPLEMENTED
;	/generate
;		size [integer!] "Key length"
;	    generator [integer!] "Generator number"
][
	make object! [
		priv-key:	;private key
		pub-key:	;public key
		g:			;generator
		p:			;prime modulus
		none
	]
]

dh-generate-key: command [
    "Generates a new DH private/public key pair using given generator(g) and modulus(p) values."
    obj [object!] "The Diffie-Hellman key object"
]

dh-compute-key: command [
    "Computes the resulting, negotiated key from a private/public key pair and the peer's public key."
    obj [object!] "The Diffie-Hellman key object"
    public-key [binary!] "Peer's public key"
]

aes: command [
	"Encrypt/decrypt data using AES algorithm. Returns stream cipher context handle or encrypted/decrypted data."
	/key
		crypt-key [binary!] "Crypt key. Have to be provided only for the first time to get stream handle!."
		iv [binary! none!] "Optional initialization vector."
	/stream
		ctx [handle!] "Stream cipher context."
		data [binary! none!] "Data to encrypt/decrypt. Or NONE to close the cipher stream."
	/decrypt "Use the crypt-key for decryption (default is to encrypt)"
]
