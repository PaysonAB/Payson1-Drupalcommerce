<?php

/* 
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
class testLogger{
    public function logIt($what) {
        if (is_array($what)) {

            $content = print_r('<pre>' . $what . '<pre>');
        } else {
            $content = print_r($what, true);
        }
        file_put_contents("c:\php\log\markolog3.txt", date("H:i:s") . "\n" . $content, FILE_APPEND);
    }
}