-- ==============================================================
-- File generated on Tue Oct 13 18:46:27 +0800 2026
-- Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
-- SW Build 2405991 on Thu Dec  6 23:38:27 MST 2018
-- IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
-- Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
-- ==============================================================
library ieee; 
use ieee.std_logic_1164.all; 
use ieee.std_logic_unsigned.all;

entity p_anonymous_nameseOg_rom is 
    generic(
             DWIDTH     : integer := 30; 
             AWIDTH     : integer := 5; 
             MEM_SIZE    : integer := 32
    ); 
    port (
          addr0      : in std_logic_vector(AWIDTH-1 downto 0); 
          ce0       : in std_logic; 
          q0         : out std_logic_vector(DWIDTH-1 downto 0);
          clk       : in std_logic
    ); 
end entity; 


architecture rtl of p_anonymous_nameseOg_rom is 

signal addr0_tmp : std_logic_vector(AWIDTH-1 downto 0); 
type mem_array is array (0 to MEM_SIZE-1) of std_logic_vector (DWIDTH-1 downto 0); 
signal mem : mem_array := (
    0 => "101101000000000000000000000000", 1 => "011010100100001010011100110010", 
    2 => "001110000010010100011101000000", 3 => "000111001000000000000100010010", 
    4 => "000011100100111000101010100110", 5 => "000001110010100011011110010101", 
    6 => "000000111001010010101000011011", 7 => "000000011100101001011011011000", 
    8 => "000000001110010100101110100101", 9 => "000000000111001010010111011010", 
    10 => "000000000011100101001011101110", 11 => "000000000001110010100101110111", 
    12 => "000000000000111001010010111100", 13 => "000000000000011100101001011110", 
    14 => "000000000000001110010100101111", 15 => "000000000000000111001010010111", 
    16 => "000000000000000011100101001100", 17 => "000000000000000001110010100110", 
    18 => "000000000000000000111001010011", 19 => "000000000000000000011100101001", 
    20 => "000000000000000000001110010101", 21 => "000000000000000000000111001010", 
    22 => "000000000000000000000011100101", 23 => "000000000000000000000001110011", 
    24 => "000000000000000000000000111001", 25 => "000000000000000000000000011101", 
    26 => "000000000000000000000000001110", 27 => "000000000000000000000000000111", 
    28 => "000000000000000000000000000100", 29 => "000000000000000000000000000010", 
    30 => "000000000000000000000000000001", 31 => "000000000000000000000000000000" );

attribute syn_rom_style : string;
attribute syn_rom_style of mem : signal is "select_rom";
attribute ROM_STYLE : string;
attribute ROM_STYLE of mem : signal is "distributed";

begin 


memory_access_guard_0: process (addr0) 
begin
      addr0_tmp <= addr0;
--synthesis translate_off
      if (CONV_INTEGER(addr0) > mem_size-1) then
           addr0_tmp <= (others => '0');
      else 
           addr0_tmp <= addr0;
      end if;
--synthesis translate_on
end process;

p_rom_access: process (clk)  
begin 
    if (clk'event and clk = '1') then
        if (ce0 = '1') then 
            q0 <= mem(CONV_INTEGER(addr0_tmp)); 
        end if;
    end if;
end process;

end rtl;

Library IEEE;
use IEEE.std_logic_1164.all;

entity p_anonymous_nameseOg is
    generic (
        DataWidth : INTEGER := 30;
        AddressRange : INTEGER := 32;
        AddressWidth : INTEGER := 5);
    port (
        reset : IN STD_LOGIC;
        clk : IN STD_LOGIC;
        address0 : IN STD_LOGIC_VECTOR(AddressWidth - 1 DOWNTO 0);
        ce0 : IN STD_LOGIC;
        q0 : OUT STD_LOGIC_VECTOR(DataWidth - 1 DOWNTO 0));
end entity;

architecture arch of p_anonymous_nameseOg is
    component p_anonymous_nameseOg_rom is
        port (
            clk : IN STD_LOGIC;
            addr0 : IN STD_LOGIC_VECTOR;
            ce0 : IN STD_LOGIC;
            q0 : OUT STD_LOGIC_VECTOR);
    end component;



begin
    p_anonymous_nameseOg_rom_U :  component p_anonymous_nameseOg_rom
    port map (
        clk => clk,
        addr0 => address0,
        ce0 => ce0,
        q0 => q0);

end architecture;


