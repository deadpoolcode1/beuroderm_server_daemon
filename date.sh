#!/bin/bash

eval "date ${1} ; am broadcast -a android.intent.action.TIME_SET"
