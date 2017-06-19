//******************************************************************************
//*
//*      Xilinx zynq7000 QSPI Support Source File
//*
//*      Version 1.0
//*
//*      Copyright (c) 2017, emb-lib Project Team
//*
//*      This file is part of the zynq7000 library project.
//*      Visit https://github.com/emb-lib/zynq7000 for new versions.
//*
//*      Permission is hereby granted, free of charge, to any person obtaining
//*      a copy of this software and associated documentation files (the
//*      "Software"), to deal in the Software without restriction, including
//*      without limitation the rights to use, copy, modify, merge, publish,
//*      distribute, sublicense, and/or sell copies of the Software, and to
//*      permit persons to whom the Software is furnished to do so, subject to
//*      the following conditions:
//*
//*      The above copyright notice and this permission notice shall be included
//*      in all copies or substantial portions of the Software.
//*
//*      THE SOFTWARE  IS PROVIDED  "AS IS", WITHOUT  WARRANTY OF  ANY KIND,
//*      EXPRESS  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//*      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//*      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//*      CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//*      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
//*      THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//*
//------------------------------------------------------------------------------


#include <z7qspi.h>

//------------------------------------------------------------------------------
void TQSpi::init(bool manmode)
{
    write_pa(QSPI_EN_REG, 0);                                          // disable QSPI module
    //clr_bits_pa(QSPI_CONFIG_REG, QSPI_MANUAL_CS_MASK | QSPI_PCS_MASK | (1ul << 11)); // turn off nCS

    //  software reset
    
    // reset QSPI clocks
    slcr_unlock();
    const uint32_t RST_CLK_MASK = QSPI_RST_CTRL_REF_RST_MASK | QSPI_RST_CTRL_CPU1X_RST_MASK;
    write_pa(QSPI_RST_CTRL_REG, RST_CLK_MASK);            
    write_pa(QSPI_RST_CTRL_REG, 0);
    slcr_lock();
    
    write_pa(QSPI_RX_THRES_REG, 1);
    write_pa(QSPI_TX_THRES_REG, 1);
    
    //  set up configuration registers
    
    clr_bits_pa(QSPI_LQSPI_CFG_REG, QSPI_LQ_MODE_MASK);  // turn off linear mode
    
    const uint32_t MAN_MODE = manmode ? (QSPI_MAN_START_EN_MASK | QSPI_MANUAL_CS_MASK) : 0;
    const uint32_t SET_MASK = QSPI_IFMODE_MASK     +     //  flash interface in Flash I/O Mode
                              MAN_MODE             +     //  
                              QSPI_PCS_MASK        +     //  set nCS to 1
                              QSPI_FIFO_WIDTH_MASK +     //  0b11: 32 bit, the only this value supported
                              QSPI_MODE_SEL_MASK   +     //  Master Mode on
                              QSPI_HOLDB_DR_MASK   +     //  
                             (1ul << QSPI_BAUD_RATE_DIV_BPOS) +
                              QSPI_CLK_PH_MASK     +     //
                              QSPI_CLK_POL_MASK;         //
        
    
    const uint32_t CLR_MASK = QSPI_BAUD_RATE_DIV_MASK +  //  set value 000: divide by 2
                              (7ul << 11)             +  //  reserved, 0
                              QSPI_ENDIAN_MASK        +  //  little endian
                              QSPI_REF_CLK_MASK;         //  reserved, must be 0
                                                         //
    CfgReg = read_pa(QSPI_CONFIG_REG);
    CfgReg &= ~CLR_MASK;
    CfgReg |=  SET_MASK;
    write_pa(QSPI_CONFIG_REG, CfgReg);
    
    write_pa(QSPI_EN_REG, 1);                            // enable QSPI module
    
    
}
//------------------------------------------------------------------------------
uint32_t TxBuf[1024];

void TQSpi::run()
{
    if(Launch)
    {
        switch(CmdIndex)
        {
        case 0: Response = read_id();         break;
        case 1: Response = read_sr();         break;
        case 2: Response = read_cr();         break;
        case 3: read(Address, Buf, Count);    break;
        case 4: wren();                       break;
        case 5: wrr(Buf[0]);                  break;
        case 6: serase(Address);              break;
        case 7: write(Address, TxBuf, Count); break;
        }
        Launch = false;
    }
} 
//------------------------------------------------------------------------------
uint16_t TQSpi::read_id()
{
    write_pa(QSPI_RX_THRES_REG, 2);
    cs_on();
    write_pa(QSPI_TXD0_REG,  cmdREAD_ID);
    write_pa(QSPI_TXD2_REG,  0);
    start_transfer();
    while( !(read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK) ) { }
    cs_off();
    read_pa(QSPI_RX_DATA_REG);
    return read_pa(QSPI_RX_DATA_REG) >> 16;
}
//------------------------------------------------------------------------------
uint8_t TQSpi::read_sr()
{
    write_pa(QSPI_RX_THRES_REG, 1);
    cs_on();
    write_pa(QSPI_TXD2_REG,  cmdRDSR1);
    start_transfer();
    while( !(read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK) ) { }
    cs_off();
    return read_pa(QSPI_RX_DATA_REG) >> 24;
}
//------------------------------------------------------------------------------
uint8_t TQSpi::read_cr()
{
    write_pa(QSPI_RX_THRES_REG, 1);
    cs_on();
    write_pa(QSPI_TXD2_REG,  cmdRDCR);
    start_transfer();
    while( !(read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK) ) { }
    cs_off();
    return read_pa(QSPI_RX_DATA_REG) >> 24;
}
//------------------------------------------------------------------------------
void TQSpi::wren()
{ 
    write_pa(QSPI_RX_THRES_REG, 1);
    cs_on();
    write_pa(QSPI_TXD1_REG,  cmdWREN); 
    start_transfer();
    while( ! (read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK) ) { }
    cs_off();
    Response = read_pa(QSPI_RX_DATA_REG);
}
//------------------------------------------------------------------------------
void TQSpi::wrr(uint16_t regs)      // regs[7:0] - SR; regs[15:8] - CR
{ 
    wren();
    write_pa(QSPI_RX_THRES_REG, 1);
    cs_on();
    write_pa(QSPI_TXD3_REG,  cmdWRR + ( regs << 8) ); 
    start_transfer();
    while( ! (read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK) ) { }
    cs_off();
    read_pa(QSPI_RX_DATA_REG);
}
//------------------------------------------------------------------------------
void TQSpi::serase(const uint32_t addr)
{
    wren();
    write_pa(QSPI_RX_THRES_REG, 1);
    cs_on();
    uint32_t rev_addr = __builtin_bswap32(addr) >> 8; 
    write_pa(QSPI_TXD0_REG,  cmdSE + ( rev_addr << 8) ); 
    start_transfer();
    while( ! (read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK) ) { }
    cs_off();
    read_pa(QSPI_RX_DATA_REG);
}
//------------------------------------------------------------------------------
void TQSpi::program_page(const uint32_t addr, const uint32_t *data)
{
    wren();
    cs_on();
    uint32_t rev_addr = __builtin_bswap32(addr) >> 8; 
    write_pa(QSPI_TXD0_REG,  cmdQPP + ( rev_addr << 8) ); 

    const uint32_t CHUNK0 = 16;
    const uint32_t CHUNK1 = PAGE_SIZE - CHUNK0;
    

    write_pa( QSPI_TX_THRES_REG, FIFO_SIZE -  CHUNK1 );
    write_tx_fifo(data, CHUNK0);

    start_transfer();

    while( !(read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_TX_FIFO_NOT_FULL_MASK) ) { }
    write_tx_fifo(data + CHUNK0, CHUNK1);

    write_pa(QSPI_TX_THRES_REG, 1);
    while( !(read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_TX_FIFO_NOT_FULL_MASK) ) { }

    cs_off();
    flush_rx_fifo();
    write_pa(GPIO_MASK_DATA_0_LSW_REG, (~(1ul << 13) << 16) | (1ul << 13) );  // JE1 on
    while( wip() ) { }
    write_pa(GPIO_MASK_DATA_0_LSW_REG, (~(1ul << 13) << 16) | 0 );            // JE1 off
}
//------------------------------------------------------------------------------
void TQSpi::write(const uint32_t addr, const uint32_t *data, const uint32_t count)
{
    const uint32_t CHUNKS = count/PAGE_SIZE;
    for(uint32_t i = 0; i < CHUNKS; ++i)
    {
        program_page(addr + i*PAGE_SIZE, data + i*PAGE_SIZE);
    }
}
//------------------------------------------------------------------------------
void TQSpi::read(const uint32_t addr, uint32_t * const dst, uint32_t count)
{
    cs_on();

    // issue command/address
    write_pa(QSPI_RX_THRES_REG, 1);
    write_pa(QSPI_TXD1_REG,  cmdQOR);
    start_transfer();
    while( !(read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK) ) { }
    read_pa(QSPI_RX_DATA_REG);      // drop command/address response

    uint32_t rev_addr = __builtin_bswap32(addr); 
    write_pa(QSPI_TXD0_REG, rev_addr >> 8 );
    start_transfer();
    while( !(read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK) ) { }
    read_pa(QSPI_RX_DATA_REG);      // drop dummy data response

    const uint32_t CHUNK_SIZE = 32; // words
          uint32_t rchunk;

    // data transfer
    uint32_t rcount = count;
    if(count > 63)
    {
        fill_tx_fifo(63);
        count -= 63;
        write_pa(QSPI_TX_THRES_REG, FIFO_SIZE - CHUNK_SIZE + 1);
        write_pa(QSPI_RX_THRES_REG, CHUNK_SIZE);
        rchunk = CHUNK_SIZE;
    }
    else 
    {
        fill_tx_fifo(count);
        write_pa(QSPI_RX_THRES_REG, count);
        rchunk = count;
        count = 0;
    }
    
    start_transfer();
    uint32_t RxIndex = 0;
    for(;;)
    {
        if( count && (read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_TX_FIFO_NOT_FULL_MASK) )
        {
            if(count > CHUNK_SIZE)
            {
                fill_tx_fifo(CHUNK_SIZE);
                count -= CHUNK_SIZE;
            }
            else 
            {
                fill_tx_fifo(count);
                count = 0;
            }
        }
        
        if( read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK )
        {
            read_rx_fifo(dst + RxIndex, rchunk);
            RxIndex += rchunk;
            rcount  -= rchunk;
            if(rcount <= 63)        
            {
                if(rcount == 0)
                {
                    Response = RxIndex;
                    break;
                }
                
                write_pa(QSPI_RX_THRES_REG, rcount);
                rchunk = rcount;
            }
        }
    }

    cs_off();
}
//------------------------------------------------------------------------------
void TQSpi::fill_tx_fifo(const uint32_t count, const uint32_t pattern)
{
    for(uint32_t i = 0; i < count; ++i)
    {
        write_pa(QSPI_TXD0_REG, pattern);
    }
}
//------------------------------------------------------------------------------
void TQSpi::write_tx_fifo(const uint32_t *data, const uint32_t count)
{
    for(uint32_t i = 0; i < count; ++i)
    {
        write_pa(QSPI_TXD0_REG, data[i]);
    }
}
//------------------------------------------------------------------------------
void TQSpi::read_rx_fifo(uint32_t * const dst, const uint32_t count)
{
    for(uint32_t i = 0; i < count; ++i)
    {
        dst[i] = read_pa(QSPI_RX_DATA_REG);
    }
}
//------------------------------------------------------------------------------
void TQSpi::flush_rx_fifo()
{
    write_pa(QSPI_RX_THRES_REG, 1);
    while( read_pa(QSPI_INT_STS_REG) & QSPI_INT_STS_RX_FIFO_NOT_EMPTY_MASK ) 
    { 
        read_pa(QSPI_RX_DATA_REG);

    }
}
//------------------------------------------------------------------------------

