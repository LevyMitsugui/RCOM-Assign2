#!/bin/bash

/system reset-configuration

/interface bridge add name=bridge50
/interface bridge add name=bridge51

/interface bridge port remove [find interface=ether1]
/interface bridge port remove [find interface=ether12]
/interface bridge port remove [find interface=ether13]
/interface bridge port remove [find interface=ether14]
/interface bridge port remove [find interface=ether24]

/interface bridge port add bridge=bridge50 interface=ether14
/interface bridge port add bridge=bridge50 interface=ether13

/interface bridge port add bridge=bridge51 interface=ether1
/interface bridge port add bridge=bridge51 interface=ether12
/interface bridge port add bridge=bridge51 interface=ether24

/interface bridge port print brief
