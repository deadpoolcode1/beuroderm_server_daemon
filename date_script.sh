#!/bin/bash
<<description
    this script mediate setting the time from C to the Android system
description
eval "date ${1} ; am broadcast -a android.intent.action.TIME_SET"
