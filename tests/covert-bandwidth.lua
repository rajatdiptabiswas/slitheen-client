--------------------------------------------------
-- Author: Cecylia Bocovich <cbocovic@uwaterloo.ca>
-- Purpose: Extracts statistics about TLS handshakes
-- Usage: tshark -q <other opts> -Xlua_script:tls_stats.lua -r <trace>
--------------------------------------------------

do
    -- Extractor definitions
    ip_addr_extractor = Field.new("ip.addr")
    tcp_src_port_extractor = Field.new("tcp.srcport")
    tcp_dst_port_extractor = Field.new("tcp.dstport")
    tcp_len_extractor = Field.new("tcp.len")
    tcp_stream_extractor = Field.new("tcp.stream")

    local function main()
        local tap = Listener.new("tcp")

        local count = 1
        local total_bytes = 0

        local file = assert(io.open("bandwidth"..tostring(count)..".csv", "w"))
        file:write("time,bytes\n")
        file:close()

        --------------------------------
        ----- Handshake Statistics -----
        --------------------------------

        -- Each stream has a table that holds the following data:
        -- {state = [SHAKING, SHOOK, APPLICATION],
        --  clnt_session_id = [Bytes], srvr_session_id = [Bytes],
        --  session_ticket = [Bytes], resumed = [Boolean],
        --  ccs_received = [Int],
        --  start_time = [Float], end_time = [Float], shake_time = [Float]}

        function stats_tls_handshake(pinfo, tvb)
            local ip_src, ip_dst = ip_addr_extractor()
            local port_src = tcp_src_port_extractor()
            local port_dst = tcp_dst_port_extractor()
            local tcp_len = tcp_len_extractor()
            -- check if stream is already saved

            if(tostring(port_src) == "1080") then
                --This packet is headed back to the browser
                if( not (tostring(tcp_len) == "0")) then
                    total_bytes = total_bytes + tonumber(tostring(tcp_len))
                    local file = assert(io.open("bandwidth"..tostring(count)..".csv", "a"))
                    file:write(tostring(pinfo.abs_ts) .. "," .. tostring(total_bytes).."\n")
                    file:close()

                end
            end

            if(tostring(port_dst) == "8888") then
                --start new file
                if(total_bytes > 0) then
                    count = count + 1
                end
                total_bytes = 0
                local file = assert(io.open("bandwidth"..tostring(count)..".csv", "w"))
                file:write("time,bytes\n")
                file:close()
            end
        end

        -- start/end times
        local start_time
        local end_time
        function stats_start_end_times(pinfo)
            if (not start_time) then
                start_time =  pinfo.abs_ts
                end_time  =  pinfo.abs_ts
            else
                if ( start_time > pinfo.abs_ts ) then start_time = pinfo.abs_ts end
                if ( end_time < pinfo.abs_ts  ) then end_time = pinfo.abs_ts end
            end
        end

-------------------
----- tap functions
-------------------
        function tap.reset()
        end

        function tap.packet(pinfo,tvb,ip)
            stats_start_end_times(pinfo)
            stats_tls_handshake(pinfo, tvb)
        end

        function tap.draw()
            --print("=== Stream Information ===")
            --print_stream_info()
            print("=== Handshake Statistics ===")
            print("Capture Start Time: " .. tostring(start_time) )
            print("Capture End Time: " .. tostring(end_time) )

        end
    end

    main()
end
