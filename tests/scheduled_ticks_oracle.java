// Dependency-free fixture model of the supplied Java 26.3 tick sources.
// It mirrors TickPriority IDs/byValue, ScheduledTick drain ordering and
// identity+position uniqueness, SavedTick wrap/unpack, LevelChunkTicks pending
// hydration/packing/callbacks/removal. This is a JVM reference model with stubs,
// not execution of Minecraft classes or a claim of full world-tick parity.
import java.util.*;
class ScheduledTicksOracle {
    enum Priority {
        EXTREMELY_HIGH(-3), VERY_HIGH(-2), HIGH(-1), NORMAL(0), LOW(1), VERY_LOW(2), EXTREMELY_LOW(3);
        final int value; Priority(int v) { value = v; }
        static Priority byValue(int value) {
            for (var p : values()) if (p.value == value) return p;
            return value < -3 ? EXTREMELY_HIGH : EXTREMELY_LOW;
        }
    }
    record Pos(int x, int y, int z) {}
    record Saved(Object type, Pos pos, int delay, Priority priority) {
        Tick unpack(long now, long order) { return new Tick(type, pos, now + delay, priority, order); }
    }
    record Tick(Object type, Pos pos, long trigger, Priority priority, long order) {
        Saved pack(long now) { return new Saved(type, pos, (int)(trigger - now), priority); }
    }
    static final Comparator<Tick> DRAIN = Comparator.comparingLong(Tick::trigger)
        .thenComparingInt(t -> t.priority.ordinal()).thenComparingLong(Tick::order);
    static final class ChunkTicks {
        final PriorityQueue<Tick> q = new PriorityQueue<>(DRAIN);
        final Set<Key> unique = new HashSet<>();
        List<Saved> pending; int callbacks;
        ChunkTicks() {}
        ChunkTicks(List<Saved> p) { pending=p; for(var t:p) unique.add(new Key(t.type,t.pos)); }
        void schedule(Tick t) { if(unique.add(new Key(t.type,t.pos))) unchecked(t); }
        void unchecked(Tick t) { q.add(t); callbacks++; }
        boolean has(Pos p,Object t) { return unique.contains(new Key(t,p)); }
        Tick poll() { var t=q.poll(); if(t!=null) unique.remove(new Key(t.type,t.pos)); return t; }
        int count() { return q.size()+(pending==null?0:pending.size()); }
        List<Tick> all() { return new ArrayList<>(q); }
        void remove(Object type) { q.removeIf(t -> { if(t.type==type){unique.remove(new Key(t.type,t.pos));return true;} return false; }); }
        List<Saved> pack(long now) {
            var out=new ArrayList<Saved>(); if(pending!=null)out.addAll(pending);
            var sorted=all(); sorted.sort(Comparator.comparingLong(Tick::order));
            for(var t:sorted)out.add(t.pack(now)); return out;
        }
        void unpack(long now) {
            if(pending==null)return; int order=-pending.size();
            for(var t:pending)unchecked(t.unpack(now,order++)); pending=null;
        }
    }
    static final class Key {
        final Object type; final Pos pos; Key(Object t,Pos p){type=t;pos=p;}
        public int hashCode(){return 31*System.identityHashCode(type)+pos.hashCode();}
        public boolean equals(Object o){return o instanceof Key k&&type==k.type&&pos.equals(k.pos);}
    }
    public static void main(String[] args) {
        for(int i=-5;i<=5;i++)System.out.print(Priority.byValue(i).ordinal()+" "); System.out.println();
        Object A=new Object(), B=new Object(); Pos p0=new Pos(1,2,3), p1=new Pos(1,2,4);
        ChunkTicks q=new ChunkTicks();
        q.schedule(new Tick(A,p0,7,Priority.LOW,1)); q.schedule(new Tick(A,p0,2,Priority.EXTREMELY_HIGH,0));
        q.schedule(new Tick(B,p0,7,Priority.NORMAL,-4)); q.schedule(new Tick(A,p1,7,Priority.VERY_HIGH,4));
        if(q.callbacks!=3||q.count()!=3||!q.has(p0,A)||q.has(p1,B))throw new AssertionError();
        // Emit a clean Java counterpart with stable labels after exercising deduplication.
        var ordered=q.all(); ordered.sort(DRAIN);
        for(var x:ordered)System.out.print((x.type==A?11:12)+":"+x.pos.x+","+x.pos.y+","+x.pos.z+":"+x.trigger+":"+x.priority.ordinal()+":"+x.order+" ");
        System.out.println();
        ChunkTicks pending=new ChunkTicks(List.of(new Saved(A,p0,3,Priority.NORMAL),new Saved(A,p0,-8,Priority.LOW),new Saved(B,p1,0,Priority.EXTREMELY_LOW)));
        pending.schedule(new Tick(A,p0,8,Priority.HIGH,9)); pending.unpack(-2);
        if(pending.callbacks!=3||pending.count()!=3)throw new AssertionError();
        var hydrated=pending.all(); hydrated.sort(DRAIN);
        for(var x:hydrated)System.out.print((x.type==A?11:12)+":"+x.trigger+":"+x.priority.ordinal()+":"+x.order+" "); System.out.println();
        var packed=pending.pack(-2); if(packed.size()!=3||packed.get(0).delay!=3||packed.get(1).delay!=-8||packed.get(2).delay!=0)throw new AssertionError();
        ChunkTicks extreme=new ChunkTicks(); extreme.schedule(new Tick(A,p0,Long.MAX_VALUE,Priority.NORMAL,Long.MAX_VALUE));
        extreme.schedule(new Tick(B,p0,Long.MIN_VALUE,Priority.NORMAL,Long.MIN_VALUE));
        var first=extreme.poll(); int wrapped=(int)(Long.MIN_VALUE-Long.MAX_VALUE);
        if(first.trigger!=Long.MIN_VALUE||wrapped!=1)throw new AssertionError();
        System.out.println("EXTREME "+first.trigger+" "+wrapped);
        ChunkTicks remove=new ChunkTicks(); remove.schedule(new Tick(A,p0,0,Priority.NORMAL,0)); remove.schedule(new Tick(B,p0,1,Priority.NORMAL,1)); remove.remove(A);
        if(remove.count()!=1||remove.has(p0,A)||!remove.has(p0,B))throw new AssertionError();
        System.out.println("PASS callbacks=6 remove=1 blackhole=0");
    }
}
