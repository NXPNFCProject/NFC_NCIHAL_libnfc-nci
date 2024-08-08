Casimir
=======

Introduction
------------

Casimir aims to provide virtual tag discovery and emulation for NFC
applications, in order to unlock NFC capability in various testing
contexes.

Casimir includes a virtual NFCC implementation, and internally emulates
RF communications between multiple connected hosts. Cf the RF packets
specification in ``src/rf_packets.pdl``.


Usage
-----

Standalone
^^^^^^^^^^

Casimir may be built and run as a standalone server.

.. sourcecode:: bash
    Usage: casimir [--nci-port <nci-port>] [--rf-port <rf-port>]

    Nfc emulator.

    Options:
      --nci-port        configure the TCP port for the NCI server.
      --rf-port         configure the TCP port for the RF server.
      --help            display usage information

Cuttlefish
^^^^^^^^^^

Cuttlefish runs casimir as a host service for emulating the NFC chipset inside
the Android phone. The port numbers for a *local instance* can be obtained from
the instance number:

- ``nci_port`` is ``7100 + instance_num - 1``
- ``rf_port`` is ``8100 + instance_num - 1``

Tests
^^^^^

The script ``scripts/t4at.py`` may be used to emulate a Type 4-A Tag device on
the RF port, in either listen or poll mode.

.. sourcecode:: bash
    usage: t4at.py [-h] [--address ADDRESS] [--rf-port RF_PORT] [--mode {poll,listen}

    options:
      -h, --help            show this help message and exit
      --address ADDRESS     Select the casimir server address
      --rf-port RF_PORT     Select the casimir TCP RF port
      --mode {poll,listen}  Select the tag mode

To run a basic tag detection test on Cuttlefish:

.. sourcecode:: bash

    # Start the cuttlefish instance (numbered 1)
    # Wait for NFC start up.
    launch_cvd

    # Create a tag device connected to the casimir instance.
    cd $ANDROID_TOP/system/nfc/tools/casimir
    ./scripts/t4at.py --rf-port 8100 --mode listen


Supported features
------------------

Casimir currently supports a subset of the NFC features described in the
following technical specification documents:

- ``[NCI]`` NFC Controller Interface Technical Specification Version 2.2
- ``[DIGITAL]`` Digital Protocol Technical Specification Version 2.3
- ``[ACTIVITY]`` Activity Technical Specification Version 2.2

Supported technologies: ``NFC_A``
Supported protocols: ``ISO_DEP``, ``NFC_DEP``
Supported RF interfaces: ``ISO_DEP``, ``NFC_DEP``

NCI Commands
^^^^^^^^^^^^
Core management
"""""""""""""""
+---------------------------------+--------------+-------------------------------------------------+
| CORE_RESET_CMD                  | Completed    |                                                 |
| CORE_RESET_RSP                  |              |                                                 |
| CORE_RESET_NTF                  |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| CORE_INIT_CMD                   | Completed    | To be defined: number and type of RF            |
| CORE_INIT_RSP                   |              | interfaces. It seems that the mandated default  |
|                                 |              | is 1 : frame RF interface.                      |
|                                 |              | To be defined: default configuration for the    |
|                                 |              | NFCC, best is to get one from a real phone      |
|                                 |              | NCI trace.                                      |
+---------------------------------+--------------+-------------------------------------------------+
| CORE_SET_CONFIG_CMD             | Completed    | The configuration is saved but currently        |
| CORE_SET_CONFIG_RSP             |              | unused. Basic validation is implemented.        |
+---------------------------------+--------------+-------------------------------------------------+
| CORE_GET_CONFIG_CMD             | Completed    |                                                 |
| CORE_GET_CONFIG_RSP             |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| CORE_CONN_CREATE_CMD            | Completed    |                                                 |
| CORE_CONN_CREATE_RSP            |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| CORE_CONN_CLOSE_CMD             | Completed    |                                                 |
| CORE_CONN_CLOSE_RSP             |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| CORE_CONN_CREDITS_NTF           | Completed    |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| CORE_GENERIC_ERROR_NTF          | Not started  | Unused in the implementation so far             |
| CORE_INTERFACE_ERROR_NTF        |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| CORE_SET_POWER_SUB_STATE_CMD    | In progress  | Implemented as stub                             |
| CORE_SET_POWER_SUB_STATE_RSP    |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+

RF management
"""""""""""""
+---------------------------------+--------------+-------------------------------------------------+
| RF_DISCOVER_MAP_CMD             | Completed    |                                                 |
| RF_DISCOVER_MAP_RSP             |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_SET_LISTEN_MODE_ROUTING_CMD  | In progress  | Implemented as stub                             |
| RF_SET_LISTEN_MODE_ROUTING_RSP  |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_GET_LISTEN_MODE_ROUTING_CMD  | In progress  | Implemented as stub                             |
| RF_GET_LISTEN_MODE_ROUTING_RSP  |              |                                                 |
| RF_GET_LISTEN_MODE_ROUTING_NTF  |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_DISCOVER_CMD                 | Completed    |                                                 |
| RF_DISCOVER_RSP                 |              |                                                 |
| RF_DISCOVER_NTF                 |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_DISCOVER_SELECT_CMD          | In progress  | Missing protocol and interface combinations     |
| RF_DISCOVER_SELECT_RSP          |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_INTF_ACTIVATED_NTF           | Completed    |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_DEACTIVATE_CMD               | In progress  |                                                 |
| RF_DEACTIVATE_RSP               |              |                                                 |
| RF_DEACTIVATE_NTF               |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_FIELD_INFO_NTF               | Not started  |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_T3T_POLLING_CMD              | Not started  |                                                 |
| RF_T3T_POLLING_RSP              |              |                                                 |
| RF_T3T_POLLING_NTF              |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_NFCEE_ACTION_NTF             | Not started  |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_NFCEE_DISCOVERY_REQ_NTF      | Completed    |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_PARAMETER_UPDATE_CMD         | Not started  |                                                 |
| RF_PARAMETER_UPDATE_RSP         |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_INTF_EXT_START_CMD           | Not started  |                                                 |
| RF_INTF_EXT_START_RSP           |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_INTF_EXT_STOP_CMD            | Not started  |                                                 |
| RF_INTF_EXT_STOP_RSP            |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_EXT_AGG_ABORT_CMD            | Not started  |                                                 |
| RF_EXT_AGG_ABORT_RSP            |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_NDEF_ABORT_CMD               | Not started  |                                                 |
| RF_NDEF_ABORT_RSP               |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_ISO_DEP_NAK_PRESENCE_CMD     | Not started  |                                                 |
| RF_ISO_DEP_NAK_PRESENCE_RSP     |              |                                                 |
| RF_ISO_DEP_NAK_PRESENCE_NTF     |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| RF_SET_FORCED_NFCEE_ROUTING_CMD | Not started  |                                                 |
| RF_SET_FORCED_NFCEE_ROUTING_RSP |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+

NFCEE management
""""""""""""""""
+---------------------------------+--------------+-------------------------------------------------+
| NFCEE_DISCOVER_CMD              | In progress  | Implemented discovery for one NFCEE (eSE (ST))  |
| NFCEE_DISCOVER_RSP              |              | reproducing the configuration found on Pixel7   |
| NFCEE_DISCOVER_NTF              |              | devices.                                        |
+---------------------------------+--------------+-------------------------------------------------+
| NFCEE_MODE_SET_CMD              | Completed    |                                                 |
| NFCEE_MODE_SET_RSP              |              |                                                 |
| NFCEE_MODE_SET_NTF              |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| NFCEE_STATUS_NTF                | Not started  |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
| NFCEE_POWER_AND_LINK_CNTRL_CMD  | Not started  |                                                 |
| NFCEE_POWER_AND_LINK_CNTRL_RSP  |              |                                                 |
+---------------------------------+--------------+-------------------------------------------------+
