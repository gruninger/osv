package com.cloudius.cli.util;

import java.io.IOException;

import com.cloudius.net.Arp;
import com.cloudius.net.IFConfig;
import com.cloudius.net.Route;

import sun.org.mozilla.javascript.ScriptableObject;
import sun.org.mozilla.javascript.annotations.JSFunction;

public class Networking extends ScriptableObject {

    // Identifies the scriptable object
    private static final long serialVersionUID = 436644325540039L;

    @JSFunction
    public static boolean set_ip(String ifname, String ip, String netmask)
    {
        try {
            IFConfig.set_ip(ifname, ip, netmask);
            return true;
        } catch (IOException e) {
            return false;
        }
    }
    
    @JSFunction
    public static boolean if_up(String ifname)
    {
        try {
            IFConfig.if_up(ifname);
            return true;
        } catch (IOException e) {
            return false;
        }
    }
    
    @JSFunction
    public static void arp_add(String ifname, String macaddr, String ip)
    {
        Arp.add(ifname, macaddr, ip);
    }
    
    @JSFunction
    public static void route_add_default(String gw)
    {
        Route.add_default(gw);
    }
    
    @Override
    public String getClassName() {
        return "Networking";
    }

}