LIGHTNING-LISTPEERS(7) Manual Page
==================================
lightning-listpeers - Command for returning data on connected lightning
nodes.

SYNOPSIS
--------

**listpeers** \[*id*\] \[*level*\]

DESCRIPTION
-----------

The **listpeers** RPC command returns data on nodes that are connected
or are not connected but have open channels with this node.

Once a connection to another lightning node has been established, using
the **connect** command, data on the node can be returned using
**listpeers** and the *id* that was used with the **connect** command.

If no *id* is supplied, then data on all lightning nodes that are
connected, or not connected but have open channels with this node, are
returned.

Supplying *id* will filter the results to only return data on a node
with a matching *id*, if one exists.

Supplying *level* will show log entries related to that peer at the
given log level. Valid log levels are "io", "debug", "info", and
"unusual".

If a channel is open with a node and the connection has been lost, then
the node will still appear in the output of the command and the value of
the *connected* attribute of the node will be "false".

The channel will remain open for a set blocktime, after which if the
connection has not been re-established, the channel will close and the
node will no longer appear in the command output.

RETURN VALUE
------------

On success, an object with a "peers" key is returned containing a list
of 0 or more objects.

Each object in the list contains the following data:
- *id* : The unique id of the peer
- *connected* : A boolean value showing the connection status
- *netaddr* : A list of network addresses the node is listening on
- *globalfeatures* : Bit flags showing supported global features (BOLT \#9)
- *localfeatures* : Bit flags showing supported local features (BOLT \#9)
- *channels* : An list of channel id’s open on the peer
- *log* : Only present if *level* is set. List logs related to the
peer at the specified *level*

If *id* is supplied and no matching nodes are found, a "peers" object
with an empty list is returned.

ERRORS
------

If *id* is not a valid public key, an error message will be returned:

    { "code" : -32602, "message" : "'id' should be a pubkey, not '...'" }

If *level* is not a valid log level, an error message will be returned:

    { "code" : -32602, "message" "'level' should be 'io', 'debug', 'info', or 'unusual', not '...'" }

AUTHOR
------

Michael Hawkins <<michael.hawkins@protonmail.com>>.

SEE ALSO
--------

lightning-connect(7)

RESOURCES
---------

Main web site: <https://github.com/ElementsProject/lightning> Lightning
RFC site (BOLT \#9):
<https://github.com/lightningnetwork/lightning-rfc/blob/master/09-features.md>

------------------------------------------------------------------------

Last updated 2019-04-30 17:12:10 CEST
