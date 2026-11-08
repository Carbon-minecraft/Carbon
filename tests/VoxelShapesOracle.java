import java.util.Arrays;

// Adapted from Java Edition 26.3 VoxelShape.collideX and findIndex, with a
// one-cell occupancy mask and explicit coordinate lists replacing engine types.
// This is a source-extracted algorithm oracle, not a compiled Minecraft class.
public final class VoxelShapesOracle {
    static final double E = 1.0E-7;
    static int find(double[] coords, double value) {
        int lo=0, hi=coords.length;
        while(lo<hi) { int mid=(lo+hi)>>>1; if(value<coords[mid]) hi=mid; else lo=mid+1; }
        return lo-1;
    }
    static double collide(double[][] coords, boolean[][][] full, int axis, double[] min, double[] max, double distance) {
        if(!full[0][0][0]) return distance;
        if(Math.abs(distance)<E) return 0.0;
        int b=(axis+1)%3, c=(axis+2)%3;
        int amin=find(coords[axis],min[axis]+E), amax=find(coords[axis],max[axis]-E);
        int bmin=Math.max(0,find(coords[b],min[b]+E));
        int bmax=Math.min(1,find(coords[b],max[b]-E)+1);
        int cmin=Math.max(0,find(coords[c],min[c]+E));
        int cmax=Math.min(1,find(coords[c],max[c]-E)+1);
        if(distance>0) {
            for(int a=amax+1;a<1;a++) for(int j=bmin;j<bmax;j++) for(int k=cmin;k<cmax;k++) {
                double d=coords[axis][a]-max[axis]; if(d>=-E) distance=Math.min(distance,d); return distance;
            }
        } else if(distance<0) {
            for(int a=amin-1;a>=0;a--) for(int j=bmin;j<bmax;j++) for(int k=cmin;k<cmax;k++) {
                double d=coords[axis][a+1]-min[axis]; if(d<=E) distance=Math.max(distance,d); return distance;
            }
        }
        return distance;
    }
    static double run(double[][] coords,int axis,double[] min,double[] max,double distance) {
        return collide(coords,new boolean[][][]{{{true}}},axis,min,max,distance);
    }
    public static void main(String[] args) {
        double[][] slab={{0.25,0.75},{0,1},{0,1}}, block={{0,1},{0,1},{0,1}};
        System.out.println("collision " + run(slab,0,new double[]{-1,.1,.1},new double[]{-.5,.9,.9},2)
            + " " + run(slab,0,new double[]{1,.1,.1},new double[]{1.5,.9,.9},-2)
            + " " + run(block,0,new double[]{-1,.1,.1},new double[]{0,.9,.9},1)
            + " " + run(slab,0,new double[]{-1,2,2},new double[]{-.5,3,3},2)
            + " " + run(slab,1,new double[]{.3,-1,.3},new double[]{.7,-.5,.7},2)
            + " " + run(slab,2,new double[]{.3,.1,-1},new double[]{.7,.9,-.5},2));
    }
}
