@echo off

rem
rem A batch script wrapper for launching ZINC.
rem
rem SDL likes to redefine main() and redirect standard output and standard
rem error into "stdout.txt" and "stderr.txt" respectively. Since ZINC
rem uses these streams to show valuable information to the user, we need
rem this wrapper script to show the output/error after ZINC exits.
rem

zinc.exe %*
if exist stdout.txt type stdout.txt
if exist stderr.txt type stderr.txt
