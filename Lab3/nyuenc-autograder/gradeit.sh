#!/bin/bash

echo "****************************************************************"
echo "* CS202 nyuenc autograder                                      *"
echo "*                                                              *"
echo "* Please note that the sample autograder only checks if your   *"
echo "* program has produced the correct output. You need to run     *"
echo "* valgrind and measure the performance yourself.               *"
echo "*                                                              *"
echo "* Also, note that these sample test cases are not exhaustive.  *"
echo "* The test cases for final grading will be different from the  *"
echo "* ones provided and will not be shared. You should test your   *"
echo "* program thoroughly before submission.                        *"
echo "*                                                              *"
echo "* Do not try to hack or exploit the autograder or the CIMS     *"
echo "* computer servers.                                            *"
echo "****************************************************************"

if [ ! -d "inputs" ] || [ ! -d "refoutputs" ] || [ ! -f "checksum.txt" ]; then
  echo -e "\e[1;31mMissing files. Please re-extract them from the original autograder archive.\e[m"
  exit 1
fi

echo -e "\e[1;33mExtracting source code...\e[m"
NYUENC_GRADING=nyuenc-grading

rm -rf $NYUENC_GRADING
mkdir $NYUENC_GRADING
if ! tar xvf nyuenc-*.tar.xz -C $NYUENC_GRADING; then
  echo -e "\e[1;31mThere was an error extracting your source code. Please copy your nyuenc-*.tar.xz archive to this directory and try again.\e[m"
  exit 1
fi


echo -e "\e[1;33mCompiling nyuenc...\e[m"
compile_error() {
  echo -e "\e[1;31mThere was an error compiling nyuenc. Please make sure your nyuenc-*.tar.xz archive contains all necessary files and try again.\e[m"
  exit 1
}

module load gcc-9.2
make -C $NYUENC_GRADING || compile_error

NYUENC="$NYUENC_GRADING/nyuenc"
[ ! -f $NYUENC ] && compile_error


echo -e "\e[1;33mVerifying input files...\e[m"
input_error() {
  echo -e "\e[1;31mInput files corrupted. Please re-extract them from the original autograder archive.\e[m"
  exit 1
}
sha1sum -c --quiet checksum.txt || input_error

echo -e "\e[1;33mRunning nyuenc...\e[m"
killall -q -9 nyuenc 2> /dev/null
MYOUTPUTS=/tmp/`ls nyuenc-*.tar.xz | cut -d. -f1`
rm -rf $MYOUTPUTS myoutputs
mkdir $MYOUTPUTS
ln -sf $MYOUTPUTS myoutputs
score=0

run_test() {
  echo -n "Test $1: "

  if [[ ! -f "refoutputs/$1.out" ]]
    then
      echo "Reference output is missing for Test Case $1. Please check and try again."
      exit 1
  fi

  timeout 30 $NYUENC ${@:3} $(for ((i=1; i<=$2; ++i)); do echo -n "inputs/$1.in "; done) > myoutputs/$1.out

  if [ $? -eq 124 ]; then
    echo -e "\e[1;31mFailed (time limit exceeded)\e[m"
  elif cmp -s -b refoutputs/$1.out myoutputs/$1.out; then
    echo -e "\e[1;32mPassed\e[m"
    score=$(($score+1))
  else
    echo -e "\e[1;31mFailed\e[m"
    echo "Expected output                                                     |   Your output"
    echo "========================================================================================================================================"
    diff -y -W138 <(xxd refoutputs/$1.out) <(xxd myoutputs/$1.out) | head
    echo "(showing first ten differences only)"
    echo
  fi
}

run_test 1 1
run_test 2 1
run_test 3 1
run_test 4 10
run_test 5 1 -j 3
run_test 6 1 -j 3
run_test 7 10 -j 3

if [ $score -eq 7 ]; then
  echo "Your program spent $(/bin/time -f "%e" $NYUENC -j 3 $(for i in {1..10}; do echo -n "inputs/7.in "; done) 2>&1 > /dev/null) seconds on Test 7."
  echo "Prof. Tang's program spent $(/bin/time -f "%e" ./refnyuenc -j 3 $(for i in {1..10}; do echo -n "inputs/7.in "; done) 2>&1 > /dev/null) seconds on Test 7."
fi

echo -e "\e[1;33mCleaning up...\e[m"
rm -rf $NYUENC_GRADING

echo -e "\e[1;33mYou passed $score out of 7 test cases.\e[m"
