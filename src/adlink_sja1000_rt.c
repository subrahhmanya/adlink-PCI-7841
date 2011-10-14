/* 
 * Driver for dual-port isolated CAN interface card
 * Copyright (C) 2011  Peter Kotvan <peter.kotvan@gmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * sja1000 init and data handling
 */

#define SJA1000_IRQ_HANDLED RTDM_IRQ_HANDLED
#define SJA1000_IRQ_NONE RTDM_IRQ_NONE

#define SJA1000_METHOD_ARGS struct pcandev *dev, struct pcanctx_rt *ctx

#define SJA1000_LOCK_DECLARE
#define SJA1000_LOCK_INIT(type)
#define SJA1000_LOCK_IRQSAVE(type) {rtdm_lockctx_t lockctx;\
                                 rtdm_lock_get_irqsave(&ctx->type, lockctx)
#define SJA1000_UNLOCK_IRQRESTORE(type) rtdm_lock_put_irqrestore(&ctx->type, lockctx);}

#define SJA1000_WAKEUP_READ() rtdm_event_signal(&ctx->in_event)
#define SJA1000_WAKEUP_WRITE() rtdm_event_signal(&ctx->out_event)
#define SJA1000_WAKEUP_EMPTY() if(result == -ENODATA)\
                                  rtdm_event_signal(&ctx->empty_event)

#define SJA1000_FUNCTION_CALL(name) name(dev,ctx)

//****************************************************************************
// CODE

int sja1000_irqhandler_rt(rtdm_irq_t *irq_context)
{
  struct pcanctx_rt *ctx;
  struct pcandev *dev;

  ctx = rtdm_irq_get_arg(irq_context, struct pcanctx_rt);
  dev = ctx->dev;

  return sja1000_irqhandler_common(dev, ctx);
}

